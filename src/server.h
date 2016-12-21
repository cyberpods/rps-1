#ifndef _RPS_SERVER_H
#define _RPS_SERVER_H

#include "core.h"
#include "string.h"
#include "config.h"

#include <uv.h>

#include <unistd.h>

struct server {
    uv_loop_t       loop;   
    uv_tcp_t        us; /* libuv tcp server */

    rps_proxy_t     proxy;
    
    struct sockaddr         listen;
    
    struct config_server    *cfg;
};

rps_status_t server_init(struct server *s, struct config_server *cs);
void server_deinit(struct server *s);
void server_run(struct server *s);

/*
 * server_init
 * server_deinit
 * server_run
 * server_stop
 *
 */

#endif