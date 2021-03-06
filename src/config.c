#include "config.h"
#include "core.h"
#include "util.h"
#include "log.h"
#include "array.h"
#include "_string.h"

#include <yaml.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>


#define CONFIG_SERVERS_NUM  3
#define CONFIG_DEFAULT_ARGS  3

#define CONFIG_ROOT_PATH    1
#define CONFIG_MIN_PATH     CONFIG_ROOT_PATH + 1
#define CONFIG_MAX_PATH     CONFIG_ROOT_PATH + 2


static  rps_status_t
config_event_next(struct config *cfg) {
    int rv;
    
    rv = yaml_parser_parse(&cfg->parser, &cfg->event);
    if (!rv) {
        log_stderr("config: failed (err %d) to get next event");
        return RPS_ERROR;
    }   

    return RPS_OK;
}

static void
config_event_done(struct config *cfg) {
    yaml_event_delete(&cfg->event);
}

static rps_status_t
config_push_scalar(struct config *cfg) {
    rps_status_t status;
    rps_str_t   *value;

    char *scalar;
    size_t length;

    scalar = (char *)cfg->event.data.scalar.value;
    length = cfg->event.data.scalar.length;
    if (length == 0) {
        return RPS_ERROR;
    }

    log_verb("push '%.*s'", length, scalar);

    value = array_push(cfg->args);
    if (value == NULL) {
        return RPS_ENOMEM;
    }
    string_init(value);

    status = string_duplicate(value, scalar, length);
    if (status != RPS_OK) {
        array_pop(cfg->args);
        return status;
    }

    return RPS_OK;   
}

static rps_str_t *
config_pop_scalar(struct config *cfg) {
    rps_str_t *value;

    value = (rps_str_t *)array_pop(cfg->args);
    log_verb("pop '%.*s'", value->len, value->data);
    return value;
}

static void
config_server_init(struct config_server *server) {
    string_init(&server->proto);
    string_init(&server->listen);
    server->port = 0;
    string_init(&server->username);
    string_init(&server->password);
}

static void
config_server_deinit(struct config_server *server) {
    string_deinit(&server->proto);
    string_deinit(&server->listen);
    string_deinit(&server->username);
    string_deinit(&server->password);
}


static rps_status_t
config_servers_init(struct config_servers *servers) {
    servers->ss = array_create(CONFIG_SERVERS_NUM, sizeof(struct config_server));  
    if (servers->ss == NULL) {
        return RPS_ENOMEM;
    }

    servers->rtimeout = 0;
    servers->ftimeout = 0;

    return RPS_OK;
}

static void
config_servers_deinit(struct config_servers *servers) {
    while (array_n(servers->ss)) {
        config_server_deinit((struct config_server *)array_pop(servers->ss));
    }
    array_destroy(servers->ss);
}

static void
config_upstream_init(struct config_upstream *upstream) {
    string_init(&upstream->proto);
}

static void
config_upstream_deinit(struct config_upstream *upstream) {
    string_deinit(&upstream->proto);
}

static rps_status_t
config_upstreams_init(struct config_upstreams *upstreams) {
    upstreams->refresh = UPSTREAM_DEFAULT_REFRESH;
    upstreams->maxreconn = UPSTREAM_DEFAULT_MAXRECONN;
    upstreams->maxretry = UPSTREAM_DEFAULT_MAXRETRY;
    string_init(&upstreams->schedule);
    upstreams->hybrid = UPSTREAM_DEFAULT_BYBRID;
    upstreams->mr1m = UPSTREAM_DEFAULT_MR1M;
    upstreams->mr1h = UPSTREAM_DEFAULT_MR1H;
    upstreams->mr1d = UPSTREAM_DEFAULT_MR1D;
    upstreams->max_fail_rate = UPSTREAM_DEFAULT_MAX_FIAL_RATE;

#ifdef SOCKS4_PROXY_SUPPORT
    upstreams->pools = array_create(2, sizeof(struct config_upstream));
#else
    upstreams->pools = array_create(3, sizeof(struct config_upstream));
#endif 

    if (upstreams->pools == NULL) {
        string_deinit(&upstreams->schedule);
        return RPS_ENOMEM;
    }

    return RPS_OK;
}

static void
config_upstreams_deinit(struct config_upstreams *upstreams) {
    string_deinit(&upstreams->schedule);
    while (array_n(upstreams->pools)) {
        config_upstream_deinit((struct config_upstream *)array_pop(upstreams->pools));
    }
    array_destroy(upstreams->pools);
}

static void 
config_api_init(struct config_api *api) {
    string_init(&api->url);
    api->timeout = 0;
}

static void
config_api_deinit(struct config_api *api) {
    string_deinit(&api->url);
}

static void
config_log_init(struct config_log *log) {
    string_init(&log->file);
    string_init(&log->level);
}

static void
config_log_deinit(struct config_log *log) {
    string_deinit(&log->file);
    string_deinit(&log->level);
}

static int
config_parse_bool(rps_str_t *str) {
    if (rps_strcmp(str, "true") == 0 ) {
        return 1;
    } else if (rps_strcmp(str, "false") == 0 ) {
        return 0;
    } else {
        return -1;
    }
}

static rps_status_t
config_handler_map(struct config *cfg, rps_str_t *key, rps_str_t *val, rps_str_t *section) {
    rps_status_t status;
    struct config_server *server;
    struct config_upstream *upstream;
    int _bool;

    status = RPS_OK;

    if (section == NULL) {
        if (rps_strcmp(key, "title") == 0 ) {
            status = string_copy(&cfg->title, val);          
        } else if (rps_strcmp(key, "pidfile") == 0) {
            status = string_copy(&cfg->pidfile, val);
        } else if (rps_strcmp(key, "daemon") == 0) {
            _bool = config_parse_bool(val);
            if (_bool < 0) {
                status  = RPS_ERROR;
            } else {
                cfg->daemon = _bool;
            }
        } else {
            status = RPS_ERROR;
        }
    } else if (rps_strcmp(section, "servers") == 0) {
        if (rps_strcmp(key, "rtimeout") == 0) {
            /*convert uint from second to millsecond*/
            cfg->servers.rtimeout = (atoi((char *)val->data)) * 1000;
        } else if (rps_strcmp(key, "ftimeout") == 0){
            cfg->servers.ftimeout = (atoi((char *)val->data)) * 1000;
        } else {
            status = RPS_ERROR;
        }
    } else if (rps_strcmp(section, "ss") == 0) {
        server = (struct config_server *)array_head(cfg->servers.ss);
        if (rps_strcmp(key, "proto") == 0) {
            status = string_copy(&server->proto, val);
        } else if (rps_strcmp(key, "listen") == 0) {
            status = string_copy(&server->listen, val);
        } else if (rps_strcmp(key, "port") == 0) {
            server->port = atoi((char *)val->data);
        } else if (rps_strcmp(key, "username") == 0) {
            status = string_copy(&server->username, val);
        } else if (rps_strcmp(key, "password") == 0) {
            status = string_copy(&server->password, val);
        } else {
            status = RPS_ERROR;
        }
    } else if (rps_strcmp(section, "upstreams") == 0) {
        if (rps_strcmp(key, "refresh") == 0) {
            cfg->upstreams.refresh = (atoi((char *)val->data)) * 1000;
        } else if (rps_strcmp(key, "schedule") == 0) {
            status = string_copy(&cfg->upstreams.schedule, val);
        } else if (rps_strcmp(key, "hybrid") == 0) {
            _bool = config_parse_bool(val);
            if (_bool < 0) {
                status  = RPS_ERROR;
            } else {
                cfg->upstreams.hybrid = (unsigned)_bool;
            }
        } else if (rps_strcmp(key, "maxreconn") == 0) { 
            cfg->upstreams.maxreconn = atoi((char *)val->data);
        } else if (rps_strcmp(key, "maxretry") == 0) { 
            cfg->upstreams.maxretry = atoi((char *)val->data);
        } else if (rps_strcmp(key, "mr1m") == 0) { 
            cfg->upstreams.mr1m = atoi((char *)val->data);
        } else if (rps_strcmp(key, "mr1h") == 0) { 
            cfg->upstreams.mr1h = atoi((char *)val->data);
        } else if (rps_strcmp(key, "mr1d") == 0) { 
            cfg->upstreams.mr1d = atoi((char *)val->data);
        } else if (rps_strcmp(key, "max_fail_rate") == 0) { 
            cfg->upstreams.max_fail_rate = atof((char *)val->data);
        } else {
            status = RPS_ERROR;
        }
    } else if (rps_strcmp(section, "pools") == 0) { 
        upstream = (struct config_upstream *)array_head(cfg->upstreams.pools);
        if (rps_strcmp(key, "proto") == 0) {
            status = string_copy(&upstream->proto, val);
        } else {
            status = RPS_ERROR;
        }
    } else if (rps_strcmp(section, "api") == 0) {
        if (rps_strcmp(key, "url") == 0) {
            status = string_copy(&cfg->api.url, val);
        } else if (rps_strcmp(key, "timeout") == 0) {
            cfg->api.timeout = atoi((char *)val->data);
        } else {
            status = RPS_ERROR;
        }
    } else if (rps_strcmp(section, "log") == 0) {
        if(rps_strcmp(key, "file") == 0) {
            status = string_copy(&cfg->log.file, val);
        } else if (rps_strcmp(key, "level") == 0) {
            status = string_copy(&cfg->log.level, val);
        } else {
            status = RPS_ERROR;
        }
    } else {
        status = RPS_ERROR;
    }

    return status;
}


static rps_status_t
config_handler(struct config *cfg, rps_str_t *section) {
    rps_status_t status;
    rps_str_t *key, *val;

    ASSERT(array_n(cfg->args) <= CONFIG_MAX_PATH);

    val = config_pop_scalar(cfg);
    key = config_pop_scalar(cfg);

    status = config_handler_map(cfg, key, val, section);
    
    string_deinit(val);
    string_deinit(key);
    return status;
}

static rps_status_t
config_load(char *filename, struct config *cfg) {
    FILE *fd;

    fd = fopen(filename, "r");
    if (fd == NULL) {
        log_stderr("config: failed to open configuration: '%s': '%s'", filename, strerror(errno));
        return RPS_ERROR;
    }
    
    cfg->args = array_create(CONFIG_DEFAULT_ARGS, sizeof(rps_str_t));
    if (cfg->args == NULL) {
        goto error;
    }

    if (config_servers_init(&cfg->servers) != RPS_OK) {
        array_destroy(cfg->args);
        goto error;
    }

    if (config_upstreams_init(&cfg->upstreams) != RPS_OK) {
        array_destroy(cfg->args);
        config_servers_deinit(&cfg->servers);
        goto error;
    }

    config_api_init(&cfg->api);

    config_log_init(&cfg->log);

    cfg->fname = filename;
    cfg->fd = fd;
    cfg->depth = 0;
    cfg->seq = 0;
    cfg->daemon= 0;
    string_init(&cfg->title);
    string_init(&cfg->pidfile);
    return RPS_OK;

error:
    log_stderr("config: initial configuration failed.");

    fclose(fd);

    return RPS_ERROR;
}

static rps_status_t
config_yaml_init(struct config *cfg) {
    int rv;
    
    rv = fseek(cfg->fd, 0L, SEEK_SET);
    if (rv < 0) {
        log_stderr("config: failed to seek to the beginning of file '%s': %s",
                cfg->fname, strerror(errno));
        return RPS_ERROR;
    }

    rv = yaml_parser_initialize(&cfg->parser);
    if (!rv) {
        log_stderr("config: failed (err %d) to initialize yaml parser",
                cfg->parser.error);
        return RPS_ERROR;
    }
    
    yaml_parser_set_input_file(&cfg->parser, cfg->fd);

    return RPS_OK;
}

void
config_yaml_deinit(struct config *cfg) {
     yaml_parser_delete(&cfg->parser);
}

static rps_status_t
config_begin_parse(struct config *cfg) {
    rps_status_t status;
    bool done;
    
    status = config_yaml_init(cfg);
    if (status != RPS_OK) {
        return status;
    }

    done = false;
    while (!done) {
        status = config_event_next(cfg);
        if (status != RPS_OK) {
            return status;
        }
        
        log_verb("next begin event %d", cfg->event.type);

        switch (cfg->event.type) {
        case YAML_STREAM_START_EVENT:
        case YAML_DOCUMENT_START_EVENT:
            break;
        case YAML_MAPPING_START_EVENT:
            cfg->depth++;
            done = true;
            break;
        default:
            NOT_REACHED();
        }

        config_event_done(cfg);
    }

    return RPS_OK;
}

static rps_status_t
config_parse_core(struct config *cfg, rps_str_t *section) {
    rps_status_t status;
    rps_str_t *node;
    struct config_server *server;
    struct config_upstream *upstream;
    bool done, leaf;

    status = config_event_next(cfg);
    if(status != RPS_OK) {
        if (section != NULL) {
            string_free(section);
        }
        return status;
    }

    log_verb("next event %d depth %d seq %d args.length %d", cfg->event.type,cfg->depth, cfg->seq, array_n(cfg->args));

    done = false;
    leaf = false;

    switch (cfg->event.type) {
    case YAML_MAPPING_START_EVENT:
        cfg->depth++;

        if (cfg->depth >= CONFIG_MIN_PATH && array_n(cfg->args)) {
            ASSERT(array_n(cfg->args) == 1);   
            if (section != NULL) {
                string_free(section);
            }
            section = string_new();
            node = config_pop_scalar(cfg);
            status = string_copy(section, node);
            string_deinit(node);
            if (status != RPS_OK) {
                break;
            }
        } 

        if (cfg->seq) {
            if (rps_strcmp(section, "ss") == 0 ) {
                /* new server block */
                server = (struct config_server *)array_push(cfg->servers.ss);
                if (server == NULL) {
                    status = RPS_ENOMEM;
                    break;
                }
                config_server_init(server);
            }

            if (rps_strcmp(section, "pools") == 0 ) {
                /* new pool block */
                upstream = (struct config_upstream *)array_push(cfg->upstreams.pools);
                if (upstream == NULL) {
                    status = RPS_ENOMEM;
                    break;
                }
                config_upstream_init(upstream);
            }
            
        }
        break;

    case YAML_MAPPING_END_EVENT:
        cfg->depth--;
        if (!cfg->seq) {
            if(section != NULL) {
                string_free(section);
                section = NULL;
            }
        }
        if (cfg->depth == 0) {
            done = true;
        }
        break;

    case YAML_SEQUENCE_START_EVENT:
        cfg->seq = 1;
        break;

    case YAML_SEQUENCE_END_EVENT:
        cfg->seq = 0;
        ASSERT(section != NULL);
        string_free(section);
        section = NULL;
        break;

    case YAML_SCALAR_EVENT:
        status = config_push_scalar(cfg);

        if (array_n(cfg->args) == CONFIG_MIN_PATH) {
            leaf = true;
        }
        break;

    default:
        NOT_REACHED();
    }

    config_event_done(cfg);
    
    if (status != RPS_OK) {
        if (section != NULL) {
            log_warn("config parse section %s error", section->data);
            string_free(section);
        }
        return status;
    }

    if (done) {
        if (section != NULL) {
            string_free(section);
        }
        return RPS_OK;
    }

    if (leaf) {
        status = config_handler(cfg, section);
        if (status != RPS_OK) {
            if (section != NULL) {
                log_warn("config parse section %s error", section->data);
                string_free(section);
            }
            return status;
        }
    }

    return config_parse_core(cfg, section);
}

static rps_status_t
config_end_parse(struct config *cfg) {
    rps_status_t status;
    bool done;

    ASSERT(cfg->depth == 0);
    
    done = false;
    while(!done) {
        status = config_event_next(cfg);
        if (status != RPS_OK) {
            return status;
        }

        log_verb("next end event %d", cfg->event.type);
        
        switch (cfg->event.type) {
        case YAML_STREAM_END_EVENT:
            done = true;
            break;
        
        case YAML_DOCUMENT_END_EVENT:
            break;
        default:
            NOT_REACHED();

        config_event_done(cfg);
        }
    }

    config_yaml_deinit(cfg);

    return RPS_OK;
}

static rps_status_t
config_parse(struct config *cfg){
    rps_status_t status;

    status = config_begin_parse(cfg);
    if (status != RPS_OK) {
        return status;
    }


    status = config_parse_core(cfg, NULL);
    if (status != RPS_OK) {
        return status;
    }
    
    status = config_end_parse(cfg);
    if (status != RPS_OK) {
        return status;
    }
    
    return RPS_OK;
}

static void
config_dump_server(void *data) {
    struct config_server *server = data;    

    log_debug("\t - proto: %s", server->proto.data);
    log_debug("\t   listen: %s", server->listen.data);
    log_debug("\t   port: %d", server->port);
    log_debug("\t   username: %s", server->username.data);
    log_debug("\t   password: %s", server->password.data);
    log_debug("");
}

static void
config_dump_upstream(void *data) {
    struct config_upstream *upstream = data;

    log_debug("\t - proto: %s", upstream->proto.data);
    log_debug("");
}

void
config_dump(struct config *cfg) {
    log_debug("[%s Configuration]", cfg->title.data);
    log_debug("pidfile: %s", cfg->pidfile.data);
    log_debug("demon: %d", cfg->daemon);

    log_debug("[servers]");
    log_debug("\t rtimeout: %d", cfg->servers.rtimeout);
    log_debug("\t ftimeout: %d", cfg->servers.ftimeout);
    log_debug("");
    array_foreach(cfg->servers.ss, config_dump_server);

    log_debug("[upstreams]");
    log_debug("\t schedule: %s", cfg->upstreams.schedule.data);
    log_debug("\t refresh: %d", cfg->upstreams.refresh/1000);
    log_debug("\t hybrid: %d", cfg->upstreams.hybrid);
    log_debug("\t maxreconn: %d", cfg->upstreams.maxreconn);
    log_debug("\t maxretry: %d", cfg->upstreams.maxretry);
    log_debug("\t mr1m: %d", cfg->upstreams.mr1m);
    log_debug("\t mr1h: %d", cfg->upstreams.mr1h);
    log_debug("\t mr1d: %d", cfg->upstreams.mr1d);
    log_debug("\t max_fail_rate: %.2f", cfg->upstreams.max_fail_rate);
    log_debug("");
    array_foreach(cfg->upstreams.pools, config_dump_upstream);

    
    log_debug("[api]");
    log_debug("\t url: %s", cfg->api.url.data);
    log_debug("\t timeout: %d", cfg->api.timeout);
    log_debug("");
    
    log_debug("[log]");
    log_debug("\t file: %s", cfg->log.file.data);
    log_debug("\t level: %s", cfg->log.level.data);
   
}

int
config_init(char *filename, struct config *cfg) {
    rps_status_t status;
    
    status = config_load(filename, cfg);
    if (status != RPS_OK) {
        return status;
    }

    status = config_parse(cfg);
    if (status != RPS_OK) {
        log_stderr("config: configuration file '%s' syntax is invalid", filename);
        fclose(cfg->fd);
        cfg->fd = NULL;
        config_deinit(cfg);
        return status;
    }

    fclose(cfg->fd);
    cfg->fd = NULL;
    
    return RPS_OK;
}

void
config_deinit(struct config *cfg) {

    string_deinit(&cfg->title);
    string_deinit(&cfg->pidfile);

    while (array_n(cfg->args)) {
        rps_str_t *arg = config_pop_scalar(cfg);
        string_deinit(arg);
    }
    array_destroy(cfg->args);

    config_servers_deinit(&cfg->servers);

    config_upstreams_deinit(&cfg->upstreams);

    config_api_deinit(&cfg->api);

    config_log_deinit(&cfg->log);
}
