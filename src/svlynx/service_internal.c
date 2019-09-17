#include "service_internal.h"

#include "uv.h"

#include "tn/allocator.h"
#include "tn/endpoint.h"
#include "tn/error.h"
#include "tn/event.h"
#include "tn/log.h"

#include "svlynx/link.h"
#include "svlynx/service.h"

uint64_t send_msgs = 0;
uint64_t send_bytes = 0;
uint64_t recv_msgs = 0;
uint64_t recv_bytes = 0;

typedef struct uv_buffer_work_req_s {
    uv_work_t uv_req;
    tn_buffer_t *tn_buffer;
    uv_stream_t *tcp_handle;
    int ret;
    int close;
} uv_buffer_work_req_t;

// --------------------------------------------------------------------------------------------------------------
void on_sigint_cb(uv_signal_t *handle, int signum)
{
    if (signum != SIGINT) return;

    tn_log_trace("received SIGINT");

    svl_service_t *service = handle->data;
    TN_ASSERT(service);

    if (svl_service_stop_signal(service)) {
        tn_log_error("failed to stop tcp service");
    }
}

// --------------------------------------------------------------------------------------------------------------
void on_close_handle_cb(uv_handle_t *handle)
{
    TN_MEM_RELEASE(handle);
}

// --------------------------------------------------------------------------------------------------------------
void on_close_link_cb(uv_handle_t *handle)
{
    TN_ASSERT(handle && handle->data);

    svl_link_t *link = handle->data;
    link->state = SVL_LINK_CLOSED;

    if (link->read_buffer) {
        tn_buffer_pool_push(&link->service->pool_read, link->read_buffer);
        link->read_buffer = NULL;
    }

    tn_event_client_close_t *evt_close;
    TN_GUARD_CLEANUP(tn_event_list_free_pop_close(&link->service->events, &evt_close));
    evt_close->client_id = link->id;
    evt_close->error_code = 0;
    evt_close->cmd_id = link->cmd_close_id;

    if (tn_event_list_ready_push(&link->service->events, evt_close)) {
        tn_log_error("push failed stranding event on link: %zu", link->id);
    }

    if (svl_link_list_close(&link->service->link_list, link)) {
        tn_log_error("list failed to close on link: %zu", link->id);
    }

cleanup:
    TN_MEM_RELEASE(handle);
}

// --------------------------------------------------------------------------------------------------------------
void on_send_cb(uv_write_t *req, int status)
{
    svl_service_write_req_t *write_req = (svl_service_write_req_t *)req;
    TN_ASSERT(write_req);
    TN_ASSERT(write_req->svl_link->service);

    if (status) {
        tn_log_uv_error(status);
    } else {
        send_msgs++;
        send_bytes += write_req->uv_buf.len;
    }

    svl_service_t *service = write_req->svl_link->service;
    tn_buffer_t *reqbuf = write_req->tn_buffer;
    write_req->tn_buffer = NULL;
    if (tn_buffer_pool_push(&service->pool_write, reqbuf)) {
        tn_log_error("push failed stranding buffer on link: %zu", write_req->svl_link->id);
    }

    if (tn_queue_spsc_push(&service->write_reqs_free, write_req)) {
        tn_log_error("push failed stranding write request: %zu on link: %zu", write_req->id, write_req->svl_link->id);
    }
}

// --------------------------------------------------------------------------------------------------------------
void on_recv_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    TN_ASSERT(handle && handle->data);

    svl_link_t *link = handle->data;
    TN_ASSERT(link);

    if (!link->read_buffer) {
        if (tn_buffer_pool_pop_back(&link->service->pool_read, &link->read_buffer)) {
            tn_log_warning("failed to pop read pool, expect UV_ENOBUFS");
            goto cleanup;
        }
    }

    buf->base = tn_buffer_write_ptr(link->read_buffer);
    buf->len = TN_BUFLEN_CAST(tn_buffer_remaining(link->read_buffer));

    return;

cleanup:
    if (buf) {
        buf->base = NULL;
        buf->len = 0;
    }
}

// --------------------------------------------------------------------------------------------------------------
void on_recv_cb(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf)
{
    TN_ASSERT(handle && handle->data);

    int ret = TN_RECV_ERROR;
    int close_link = 0;
    uv_buffer_work_req_t *work_req = NULL;
    svl_link_t *link = handle->data;
    TN_ASSERT(link);

    if (nread < 0) {
        /* nread is an errno when < 0 */
        ret = (int)nread;
        close_link = 1;
        goto cleanup;
    } else if (nread == 0 || !buf) {
        /* this is technically not an error */
        goto cleanup;
    }

    if (nread >= SVL_SERVICE_MAX_RECV) {
        tn_log_error("read full %zd buffer on client: %zu", nread, link->id);
    }

    recv_msgs++;
    recv_bytes += nread;
    ret = TN_RECV_EBUF;
    TN_GUARD_CLEANUP(tn_buffer_add_length(link->read_buffer, nread));

    tn_event_client_read_t *evt;
    ret = TN_RECV_EVTPOP;
    TN_GUARD_CLEANUP(tn_event_list_free_pop_read(&link->service->events, &evt));

    evt->client_id = link->id;
    evt->priv = link->read_buffer;
    evt->buffer = tn_buffer_read_ptr(link->read_buffer);
    evt->len = TN_BUFLEN_CAST(tn_buffer_read_length(link->read_buffer));

    ret = TN_RECV_EVTPUSH;
    TN_GUARD_CLEANUP(tn_event_list_ready_push(&link->service->events, evt));

    /* successful recv */
    link->read_buffer = NULL;

    return;

cleanup:
    link->error_code = ret;
    if (close_link) {
        if (!uv_is_closing((uv_handle_t *)handle)) {
            link->state = SVL_LINK_CLOSING;
            uv_close((uv_handle_t *)handle, on_close_link_cb);
        }
    }

    TN_MEM_RELEASE(work_req);
}

// --------------------------------------------------------------------------------------------------------------
void on_inbound_connection_cb(uv_stream_t *server_handle, int status)
{
    TN_ASSERT(server_handle && server_handle->data);

    int ret = UV_EINVAL;
    int handle_is_init = 0;
    uv_tcp_t *client_handle = NULL;
    svl_link_t *link = NULL;

    if (status < 0) {
        ret = status;
        goto cleanup;
    }

    svl_service_t *service = server_handle->data;
    svl_service_priv_t *priv = service->priv;
    TN_ASSERT(priv);

    ret = UV_ENOMEM;
    TN_GUARD_NULL_CLEANUP(client_handle = TN_MEM_ACQUIRE(sizeof(*client_handle)));
    TN_GUARD_CLEANUP(ret = uv_tcp_init(priv->uv_loop, client_handle));
    handle_is_init = 1;
    TN_GUARD_CLEANUP(ret = uv_accept(server_handle, (uv_stream_t *)client_handle));

    TN_GUARD_CLEANUP(ret = svl_link_list_open(&service->link_list, &link));
    client_handle->data = link;
    link->priv = client_handle;
    link->service = service;
    link->read_state = SVL_LINK_READ_HEADER;
    link->state = SVL_LINK_OPEN;
    link->error_code = 0;
    link->read_buffer = NULL;
    link->last_msg_id = 0;

    struct sockaddr_storage addr;
    int addrlen = sizeof(addr);
    TN_GUARD_CLEANUP(ret = uv_tcp_getpeername(client_handle, (struct sockaddr *)&addr, &addrlen));
    TN_GUARD_CLEANUP(ret = tn_endpoint_convert_from(&link->endpoint_remote, &addr));
    TN_GUARD_CLEANUP(ret = uv_tcp_getsockname(client_handle, (struct sockaddr *)&addr, &addrlen));
    TN_GUARD_CLEANUP(ret = tn_endpoint_convert_from(&link->endpoint_remote, &addr));

    tn_event_client_open_t *open_evt;
    TN_GUARD_CLEANUP(ret = tn_event_list_free_pop_open(&service->events, &open_evt));
    open_evt->type = TN_EVENT_CLIENT_OPEN;
    open_evt->client_id = link->id;
    open_evt->host = link->endpoint_remote;
    TN_GUARD_CLEANUP(ret = tn_event_list_ready_push(&service->events, open_evt));

    TN_GUARD_CLEANUP(ret = uv_read_start((uv_stream_t *)client_handle, on_recv_alloc_cb, on_recv_cb));

    return;

cleanup:
    tn_log_error("inbound connection error: %d -- %s -- %s", ret, uv_err_name(ret), uv_strerror(ret));
    if (client_handle) {
        if (link) {
            link->state = SVL_LINK_CLOSING;
            uv_close((uv_handle_t *)client_handle, on_close_link_cb);
        } else if (handle_is_init) {
            uv_close((uv_handle_t *)client_handle, on_close_handle_cb);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------
void on_outbound_connection_cb(uv_connect_t *connect_req, int status)
{
    TN_ASSERT(connect_req && connect_req->data);

    int ret;
    if (status < 0) {
        ret = status;
        goto cleanup;
    }

    svl_link_t *link = connect_req->data;
    uv_stream_t *client_handle = link->priv;
    TN_ASSERT(client_handle && client_handle == connect_req->handle);
    svl_service_t *service = link->service;
    TN_ASSERT(service);
    svl_service_priv_t *priv = service->priv;
    TN_ASSERT(priv);

    struct sockaddr_storage addr;
    int addrlen = sizeof(addr);
    TN_GUARD_CLEANUP(ret = uv_tcp_getpeername((uv_tcp_t *)client_handle, (struct sockaddr *)&addr, &addrlen));
    TN_GUARD_CLEANUP(ret = tn_endpoint_convert_from(&link->endpoint_remote, &addr));
    TN_GUARD_CLEANUP(ret = uv_tcp_getsockname((uv_tcp_t *)client_handle, (struct sockaddr *)&addr, &addrlen));
    TN_GUARD_CLEANUP(ret = tn_endpoint_convert_from(&link->endpoint_remote, &addr));

    tn_event_client_open_t *open_evt;
    TN_GUARD_CLEANUP(ret = tn_event_list_free_pop_open(&service->events, &open_evt));
    open_evt->type = TN_EVENT_CLIENT_OPEN;
    open_evt->client_id = link->id;
    open_evt->host = link->endpoint_remote;
    open_evt->cmd_id = link->cmd_open_id;
    TN_GUARD_CLEANUP(ret = tn_event_list_ready_push(&service->events, open_evt));

    link->state = SVL_LINK_OPEN;
    TN_GUARD_CLEANUP(ret = uv_read_start((uv_stream_t *)client_handle, on_recv_alloc_cb, on_recv_cb));

    TN_MEM_RELEASE(connect_req);
    return;

cleanup:
    tn_log_error("inbound connection error: %d -- %s -- %s", ret, uv_err_name(ret), uv_strerror(ret));
    TN_MEM_RELEASE(connect_req);
    link->state = SVL_LINK_CLOSING;
    uv_close((uv_handle_t *)client_handle, on_close_link_cb);
}

// --------------------------------------------------------------------------------------------------------------
void on_timer_cb(uv_timer_t *handle)
{
    TN_ASSERT(handle && handle->data);

    svl_service_t *service = handle->data;
    svl_service_priv_t *priv = service->priv;
    TN_ASSERT(priv);

    if (svl_service_state(service) != SVL_SERVICE_STARTED) {
        if (!uv_is_closing((uv_handle_t *)priv->tcp_handle)) uv_close((uv_handle_t *)priv->tcp_handle, NULL);
        if (!uv_is_closing((uv_handle_t *)priv->uv_accept_timer)) uv_close((uv_handle_t *)priv->uv_accept_timer, NULL);
        if (!uv_is_closing((uv_handle_t *)priv->uv_prep)) uv_close((uv_handle_t *)priv->uv_prep, NULL);
        if (!uv_is_closing((uv_handle_t *)priv->uv_check)) uv_close((uv_handle_t *)priv->uv_check, NULL);
        if (!uv_is_closing((uv_handle_t *)priv->uv_signal)) uv_close((uv_handle_t *)priv->uv_signal, NULL);
    }

    return;
}

// --------------------------------------------------------------------------------------------------------------
void on_prep_cb(uv_prepare_t *handle)
{
    TN_ASSERT(handle && handle->data);

    int ret;
    svl_service_write_req_t *send_req = NULL;
    svl_service_t *service = handle->data;

    while (!tn_queue_spsc_pop_back(&service->write_reqs_ready, (void **)&send_req)) {
        TN_ASSERT(send_req);

        if (send_req->svl_link->state != SVL_LINK_OPEN) {
            tn_buffer_pool_push(&service->pool_write, send_req->tn_buffer);
            tn_queue_spsc_push(&service->write_reqs_free, send_req);
            continue;
        }

        if ((ret = uv_write((uv_write_t *)send_req, (uv_stream_t *)send_req->svl_link->priv, &send_req->uv_buf, 1, on_send_cb))) {
            tn_log_error("IO send failed:  %d -- %s -- %s", ret, uv_err_name(ret), uv_strerror(ret));
            tn_buffer_t *reqbuf = send_req->tn_buffer;
            send_req->tn_buffer = NULL;
            tn_buffer_pool_push(&service->pool_write, reqbuf);
            tn_queue_spsc_push(&service->write_reqs_free, send_req);
        }
    }
}

// private ------------------------------------------------------------------------------------------------------
static inline void handle_cmd_open(svl_service_t *service, tn_cmd_client_open_t *cmd_open)
{
    int ret = TN_ERROR_INVAL;
    int handle_is_init = 0;
    uv_tcp_t *client_handle = NULL;
    uv_connect_t *connect_req = NULL;
    svl_service_priv_t *priv = service->priv;
    svl_link_t *link = NULL;

    ret = TN_ERROR_NOMEM;
    TN_GUARD_NULL_CLEANUP(client_handle = TN_MEM_ACQUIRE(sizeof(*client_handle)));
    TN_GUARD_CLEANUP(ret = uv_tcp_init(priv->uv_loop, client_handle));
    handle_is_init = 1;

    TN_GUARD_NULL_CLEANUP(connect_req = TN_MEM_ACQUIRE(sizeof(*connect_req)));

    TN_GUARD_CLEANUP(ret = svl_link_list_open(&service->link_list, &link));
    link->priv = client_handle;
    link->service = service;
    link->read_state = SVL_LINK_READ_HEADER;
    link->state = SVL_LINK_NEW;
    link->error_code = 0;
    link->read_buffer = NULL;
    link->cmd_open_id = cmd_open->id;

    connect_req->data = link;
    client_handle->data = link;
    TN_GUARD_CLEANUP(ret = uv_tcp_connect((void *)connect_req, client_handle, (struct sockaddr *)&cmd_open->endpoint, on_outbound_connection_cb));

    return;

cleanup:
    tn_log_error("output connection error: %d -- %s -- %s", ret, uv_err_name(ret), uv_strerror(ret));
    TN_MEM_RELEASE(connect_req);
    if (link) {
        if (svl_link_list_close(&service->link_list, link)) {
            tn_log_error("failed to close link: %llu", link->id);
        }
    }
    if (client_handle) {
        uv_close((uv_handle_t *)client_handle, on_close_handle_cb);
    }
}

// private ------------------------------------------------------------------------------------------------------
static inline void handle_cmd_close(svl_service_t *service, tn_cmd_client_close_t *cmd_close)
{
    int ret = TN_ERROR_INVAL;
    svl_link_t *link = NULL;
    TN_GUARD_CLEANUP(ret = svl_link_list_get(&service->link_list, cmd_close->client_id, &link));

    TN_GUARD_CLEANUP(!link->priv || link->state != SVL_LINK_OPEN || uv_is_closing((uv_handle_t *)link->priv));

    link->cmd_close_id = cmd_close->id;
    link->state = SVL_LINK_CLOSING;
    uv_close((uv_handle_t *)link->priv, on_close_link_cb);

    return;

cleanup:
    tn_log_error("failed to close connection: %d -- %s -- %s", ret, uv_err_name(ret), uv_strerror(ret));
}

// private ------------------------------------------------------------------------------------------------------
static inline void handle_cmd_send(svl_service_t *service, tn_cmd_client_send_t *cmd_send)
{
    int ret = TN_ERROR_INVAL;
    svl_link_t *link = NULL;
    TN_GUARD_CLEANUP(ret = svl_link_list_get(&service->link_list, cmd_send->client_id, &link));

    TN_GUARD_CLEANUP(!link->priv || link->state != SVL_LINK_OPEN || uv_is_closing((uv_handle_t *)link->priv));

    uv_close((uv_handle_t *)link->priv, on_close_link_cb);

    return;

cleanup:
    tn_log_error("failed to close connection: %d -- %s -- %s", ret, uv_err_name(ret), uv_strerror(ret));
}

// --------------------------------------------------------------------------------------------------------------
void on_check_cb(uv_check_t *handle)
{
    TN_ASSERT(handle && handle->data);
    svl_service_t *service = handle->data;
    svl_service_priv_t *priv = service->priv;

    tn_cmd_base_t *cmd_base;
    while (!tn_cmd_list_ready_pop_back(&service->cmds, &cmd_base)) {
        TN_ASSERT(cmd_base);

        switch (cmd_base->type) {
        case TN_CMD_CLIENT_OPEN:
            handle_cmd_open(service, (tn_cmd_client_open_t *)cmd_base);
            break;
        case TN_CMD_CLIENT_CLOSE:
            handle_cmd_close(service, (tn_cmd_client_close_t *)cmd_base);
            break;
        case TN_CMD_CLIENT_SEND:
            tn_log_error("TN_CMD_CLIENT_SEND not implemented");
            break;
        }

        if (tn_cmd_list_free_push(&service->cmds, cmd_base)) {
            tn_log_error("error pushing command back to free list: %llu", cmd_base->id);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------
void on_walk_cb(uv_handle_t *handle, void *arg)
{
    if (!uv_is_closing(handle)) {
        tn_log_warning("Manually closing handle: %p -- %s", handle, uv_handle_type_name(uv_handle_get_type(handle)));
        uv_close(handle, NULL);
    }
}
