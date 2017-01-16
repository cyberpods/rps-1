
#include "s5.h"
#include "core.h"
#include "util.h"
#include "log.h"
#include "server.h"

#include <stdio.h>
#include <uv.h>

#define SOCKS5_VERSION  5


static s5_err_t 
s5_parse(s5_handle_t *handle, uint8_t **data, ssize_t *size) {
    s5_err_t err;
    uint8_t *p;
    uint8_t c;
    size_t i;
    size_t n;

    i = 0;
    n = *size; 
    p = *data;

    int x = 0;

    while(i < n) {
        c = p[i];
        i++;

        printf("read: %x\n", c);
        
        switch (handle->state) {
            case s5_version:
                if (c != SOCKS5_VERSION) {
                    err = s5_bad_version;
                    goto out;
         
                }
                handle->version = c;
                handle->state = s5_nmethods;
                break;

            case s5_nmethods:
                handle->nmethods = c;
                handle->__n = 0;
                handle->state = s5_methods;
                break;

            case s5_methods:
                if (handle->__n < handle->nmethods) {
                    switch (c) {
                        case 0:
                            handle->methods[handle->__n] = s5_auth_none;
                            break;
                        case 1:
                            handle->methods[handle->__n] = s5_auth_gssapi;
                            break;
                        case 2:
                            handle->methods[handle->__n]= s5_auth_passwd;
                            break;
                        default:
                            /* Unsupport auth method */
                            break;
                    }
                    handle->__n++;
                }
                break;
        }
    }

    err = s5_ok;

out:
    *data = p + i;
    *size = n - i;  
    return err;  
    
}

static uint8_t
s5_select_auth(struct s5_method_request *req) {
    int i;
    uint8_t method;

    ASSERT(req->nmethods <= 255);

    /* Select no authentication required if rps server didn't set user and password */

    for (i=0; i<req->nmethods; i++) {
        method = req->methods[i];
        if (method == s5_auth_none) {
            return s5_auth_none;
        }
    }

    return s5_auth_unacceptable;
}

static uint16_t
s5_do_handshake(struct context *ctx, uint8_t *data, ssize_t size) {
    uint16_t new_state;
    struct server *s;
    struct s5_method_request *req;
    struct s5_method_response resp;
    rps_status_t status;

    req = (struct s5_method_request *)data;
    if (req->version != SOCKS5_VERSION) {
        log_error("s5 handshake error: bad protocol version.");
        return c_kill;
    }
    
    resp.version = req->version;

    s = ctx->sess->server;
    
    if (string_empty(&s->cfg->username) && string_empty(&s->cfg->password)) {
        /* If rps didn't assign username and password, 
         * select auth method dependent on client request 
         * */
        resp.method = s5_select_auth(req);
    } else {
        resp.method = s5_auth_passwd;
    }

    switch (resp.method) {
        case s5_auth_none:
            new_state = c_request;
            break;
        case s5_auth_passwd:
            new_state = c_auth;
            break;
        case s5_auth_gssapi:
        case s5_auth_unacceptable:
            new_state = c_kill;
            log_error("s5 handshake error: unacceptable authentication.");
            break;
    }

    status = server_write(ctx, &resp, sizeof(resp));
    if (status != RPS_OK) {
        new_state = c_kill;
    }

    return new_state;
}

static uint16_t
s5_do_auth(uint8_t *data, ssize_t size) {

}

static uint16_t
s5_do_request(uint8_t *data, ssize_t size) {

}

static s5_state_t
s5_reply() {

}

void 
s5_server_do_next(struct context *ctx) {
    uint8_t    *data;
    ssize_t size;
    uint16_t new_state; 
    s5_handle_t *handle;

    handle = &ctx->proxy_handle.s5;

    data = (uint8_t *)ctx->buf;
    size = ctx->nread;

    switch (ctx->state) {
        case c_handshake:
            new_state = s5_do_handshake(ctx, data, size);
            break;
        case c_auth:
            new_state = s5_do_auth(data, size);
            break;
        case c_request:
            new_state = s5_do_request(data, size);
            break;
        default:
            NOT_REACHED();
    }
    
    ctx->state = new_state;
}
