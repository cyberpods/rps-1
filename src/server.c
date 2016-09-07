#include "server.h"
#include "core.h"
#include "string.h"
#include "util.h"

rpg_status_t
server_init(struct server *s, struct config_server *cs) {
    uv_tcp_t *us;
    int err;
    rpg_status_t status;

    err = uv_loop_init(&s->loop);
    if (err != 0) {
        UV_SHOW_ERROR(err, "loop init");
        return RPG_ERROR;
    }

    err = uv_tcp_init(&s->loop, &s->us);
    if (err !=0 ) {
        UV_SHOW_ERROR(err, "tcp init");
        return RPG_ERROR;
    }

    status = string_copy(&s->proxy, &cs->proxy);
    if (status != RPG_OK) {
        return status;
    }
    
    printf("%s listen on %s:%d\n", cs->proxy.data, cs->listen.data, cs->port);
    return RPG_OK;
}


void
server_deinit(struct server *s) {

}
