#ifndef SVL_SERVICE_INTERNAL_H
#define SVL_SERVICE_INTERNAL_H

#include <stdint.h>

#include "uv.h"

#include "svlynx/service.h"

// priv tcp service impl
typedef struct svl_service_priv_s {
    uv_loop_t *uv_loop;
    uv_tcp_t *tcp_handle;
    uv_timer_t *uv_accept_timer;
    uv_prepare_t *uv_prep;
    uv_check_t *uv_check;
    uv_signal_t *uv_signal;
} svl_service_priv_t;

typedef struct svl_service_write_req_s {
    uv_write_t uv_req;
    uv_buf_t uv_buf;
    svl_link_t *svl_link;
    tn_buffer_t *tn_buffer;
    uint64_t id;
} svl_service_write_req_t;

void on_sigint_cb(uv_signal_t *handle, int signum);
void on_close_release_cb(uv_handle_t *handle);
void on_send_cb(uv_write_t *req, int status);
void on_recv_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void on_recv_cb(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf);
void on_inbound_connection_cb(uv_stream_t *server_handle, int status);
void on_outbound_connection_cb(uv_connect_t *connect_req, int status);
void on_timer_cb(uv_timer_t *handle);
void on_prep_cb(uv_prepare_t *handle);
void on_check_cb(uv_check_t *handle);
void on_walk_cb(uv_handle_t *handle, void *arg);

#endif
