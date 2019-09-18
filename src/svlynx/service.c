#include "svlynx/service.h"

#include <stdlib.h>

#include "tn/allocator.h"
#include "tn/endpoint.h"
#include "tn/error.h"
#include "tn/log.h"

#include "svlynx/link.h"
#include "service_internal.h"

// private ------------------------------------------------------------------------------------------------------
void svl_service_set_state(svl_service_t *service, svl_service_state_t state)
{
    TN_ASSERT(service);
    tn_atomic_store(&service->state, state);
}

// private ------------------------------------------------------------------------------------------------------
void svl_service_io_thread(void *data)
{
    TN_ASSERT(data);

    svl_service_t *service = data;
    if (!service) return;

    svl_service_priv_t *priv = service->priv;
    if (!priv) return;

    int ret;
    priv->uv_loop = NULL;
    priv->tcp_handle = NULL;
    priv->uv_accept_timer = NULL;
    priv->uv_prep = NULL;
    priv->uv_check = NULL;

    ret = UV_ENOMEM;
    TN_GUARD_NULL_CLEANUP(priv->uv_loop = TN_MEM_ACQUIRE(sizeof(*priv->uv_loop)));
    TN_GUARD_NULL_CLEANUP(priv->tcp_handle = TN_MEM_ACQUIRE(sizeof(*priv->tcp_handle)));
    TN_GUARD_NULL_CLEANUP(priv->uv_accept_timer = TN_MEM_ACQUIRE(sizeof(*priv->uv_accept_timer)));
    TN_GUARD_NULL_CLEANUP(priv->uv_prep = TN_MEM_ACQUIRE(sizeof(*priv->uv_prep)));
    TN_GUARD_NULL_CLEANUP(priv->uv_check = TN_MEM_ACQUIRE(sizeof(*priv->uv_check)));
    TN_GUARD_NULL_CLEANUP(priv->uv_signal = TN_MEM_ACQUIRE(sizeof(*priv->uv_signal)));

    TN_GUARD_CLEANUP(ret = uv_loop_init(priv->uv_loop));

    TN_GUARD_CLEANUP(ret = uv_timer_init(priv->uv_loop, priv->uv_accept_timer));
    priv->uv_accept_timer->data = service;
    TN_GUARD_CLEANUP(ret = uv_timer_start(priv->uv_accept_timer, on_timer_cb, 100, 100));

    TN_GUARD_CLEANUP(ret = uv_prepare_init(priv->uv_loop, priv->uv_prep));
    priv->uv_prep->data = service;
    TN_GUARD_CLEANUP(ret = uv_prepare_start(priv->uv_prep, on_prep_cb));

    TN_GUARD_CLEANUP(ret = uv_check_init(priv->uv_loop, priv->uv_check));
    priv->uv_check->data = service;
    TN_GUARD_CLEANUP(ret = uv_check_start(priv->uv_check, on_check_cb));

    TN_GUARD_CLEANUP(ret = uv_signal_init(priv->uv_loop, priv->uv_signal));
    priv->uv_signal->data = service;
    TN_GUARD_CLEANUP(ret = uv_signal_start_oneshot(priv->uv_signal, on_sigint_cb, SIGINT));

    const uint16_t af = tn_endpoint_af_get(&service->host_listen);
    if (af == AF_INET || af == AF_INET6) {
        char ep_ip[255];
        uint16_t ep_port;
        const int backlog = (service->client_count_max < UINT16_MAX) ? (int)service->client_count_max : (int)UINT16_MAX;

        TN_GUARD_CLEANUP(ret = uv_tcp_init(priv->uv_loop, priv->tcp_handle));
        priv->tcp_handle->data = service;
        TN_GUARD_CLEANUP(ret = uv_tcp_nodelay(priv->tcp_handle, 1));
        TN_GUARD_CLEANUP(ret = uv_tcp_bind(priv->tcp_handle, (const struct sockaddr *)&service->host_listen, 0));
        TN_GUARD_CLEANUP(ret = uv_listen((uv_stream_t *)priv->tcp_handle, backlog, on_inbound_connection_cb));

        TN_GUARD_CLEANUP(tn_endpoint_string_get(&service->host_listen, &ep_port, ep_ip, sizeof(ep_ip)));
        tn_log_trace("Listening on %s:%u", ep_ip, ep_port);
    } else {
        tn_log_trace("No listener");
        priv->tcp_handle = NULL;
    }

    if (svl_service_state(service) == SVL_SERVICE_STARTING) {
        tn_log_trace("Setting service state to: SVL_SERVICE_STARTED");
        svl_service_set_state(service, SVL_SERVICE_STARTED);
        ret = uv_run(priv->uv_loop, UV_RUN_DEFAULT);
    }
    tn_log_trace("Exited uv_run loop");

    if ((ret = uv_loop_close(priv->uv_loop))) {
        tn_log_warning("Walking handles due to unclean IO loop close");
        uv_walk(priv->uv_loop, on_walk_cb, NULL);
        TN_GUARD_CLEANUP(ret = uv_loop_close(priv->uv_loop));
    }

    ret = TN_SUCCESS;

cleanup:
    if (ret != TN_SUCCESS) {
        svl_service_set_state(service, SVL_SERVICE_ERROR);
        tn_log_uv_error(ret);
    }


    if (priv->uv_check) TN_MEM_RELEASE(priv->uv_check);
    if (priv->uv_prep) TN_MEM_RELEASE(priv->uv_prep);
    if (priv->uv_accept_timer) TN_MEM_RELEASE(priv->uv_accept_timer);
    if (priv->tcp_handle) TN_MEM_RELEASE(priv->tcp_handle);
    if (priv->uv_loop) TN_MEM_RELEASE(priv->uv_loop);

    return;
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_setup(svl_service_t *service, size_t max_clients)
{
    TN_ASSERT(service);
    TN_ASSERT(max_clients);

    int ret = UV_EINVAL;
    memset(service, 0, sizeof(*service));

    service->client_count_max = max_clients;
    svl_service_set_state(service, SVL_SERVICE_NEW);

    TN_GUARD_NULL(service->priv = TN_MEM_ACQUIRE(sizeof(svl_service_priv_t)));
    memset(service->priv, 0, sizeof(svl_service_priv_t));

    TN_GUARD_CLEANUP(ret = svl_link_list_setup(&service->link_list, service->client_count_max));

    service->buffer_count = service->client_count_max * 4;
    TN_GUARD_CLEANUP(tn_buffer_pool_setup(&service->pool_read, service->buffer_count, SVL_SERVICE_MAX_RECV));
    TN_GUARD_CLEANUP(tn_buffer_pool_setup(&service->pool_write, service->buffer_count, SVL_SERVICE_MAX_RECV));
    TN_GUARD_CLEANUP(tn_cmd_list_setup(&service->cmds, service->buffer_count));
    TN_GUARD_CLEANUP(tn_event_list_setup(&service->events, service->buffer_count));
    TN_GUARD_NULL_CLEANUP(service->event_updates = TN_MEM_ACQUIRE(service->buffer_count * sizeof(tn_event_base_t *)));
    TN_GUARD_NULL_CLEANUP(service->write_reqs = TN_MEM_ACQUIRE(service->buffer_count * sizeof(*service->write_reqs)));
    TN_GUARD_CLEANUP(tn_queue_spsc_setup(&service->write_reqs_free, service->buffer_count));
    TN_GUARD_CLEANUP(tn_queue_spsc_setup(&service->write_reqs_ready, service->buffer_count));

    svl_service_write_req_t *write_req;
    for (uint64_t i = 0; i < service->buffer_count; i++) {
        write_req = &service->write_reqs[i];
        write_req->id = i;
        write_req->tn_buffer = NULL;
        TN_GUARD_CLEANUP(tn_queue_spsc_push(&service->write_reqs_free, write_req));

        service->event_updates[i] = NULL;
    }

    return TN_SUCCESS;

cleanup:
    TN_MEM_RELEASE(service->event_updates);
    TN_MEM_RELEASE(service->write_reqs);
    TN_MEM_RELEASE(service->priv);

    return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
void svl_service_cleanup(svl_service_t *service)
{
    TN_ASSERT(service);

    if (!service) return;
    if (!service->priv) return;

    svl_link_list_cleanup(&service->link_list);
    tn_buffer_pool_cleanup(&service->pool_read);
    tn_buffer_pool_cleanup(&service->pool_write);
    tn_cmd_list_cleanup(&service->cmds);
    tn_event_list_cleanup(&service->events);
    tn_queue_spsc_cleanup(&service->write_reqs_free);
    tn_queue_spsc_cleanup(&service->write_reqs_ready);

    TN_MEM_RELEASE(service->event_updates);
    TN_MEM_RELEASE(service->write_reqs);
    TN_MEM_RELEASE(service->priv);
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_listen(svl_service_t *service, tn_endpoint_t *endpoint)
{
    TN_ASSERT(service);
    TN_ASSERT(endpoint);

    svl_service_state_t state = svl_service_state(service);
    TN_GUARD_CLEANUP(state != SVL_SERVICE_NEW && state != SVL_SERVICE_STOPPED && state != SVL_SERVICE_ERROR);

    service->host_listen = *endpoint;

    return TN_SUCCESS;

cleanup:
    return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_start(svl_service_t *service)
{
    TN_ASSERT(service);

    svl_service_state_t state = svl_service_state(service);
    switch (state) {
    case SVL_SERVICE_STARTING:
    case SVL_SERVICE_STARTED:
        return TN_SUCCESS;
    case SVL_SERVICE_NEW:
    case SVL_SERVICE_STOPPED:
        break;
    case SVL_SERVICE_STOPPING:
    case SVL_SERVICE_ERROR:
    case SVL_SERVICE_INVALID:
        return TN_ERROR;
    }

    svl_service_set_state(service, SVL_SERVICE_STARTING);
    TN_GUARD(tn_thread_launch(&service->thread_io, svl_service_io_thread, service));

    return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_stop(svl_service_t *service)
{
    TN_ASSERT(service);

    svl_service_state_t state = svl_service_state(service);
    switch (state) {
    case SVL_SERVICE_NEW:
    case SVL_SERVICE_STOPPING:
    case SVL_SERVICE_STOPPED:
        return TN_SUCCESS;
    case SVL_SERVICE_STARTING:
    case SVL_SERVICE_STARTED:
        break;
    case SVL_SERVICE_ERROR:
    case SVL_SERVICE_INVALID:
        return TN_ERROR;
    }

    tn_log_trace("tcp service stopping");
    svl_service_set_state(service, SVL_SERVICE_STOPPING);

    if (tn_thread_join(&service->thread_io)) {
        tn_log_error("error joining IO thread");
        svl_service_set_state(service, SVL_SERVICE_ERROR);
        return TN_ERROR;
    }

    svl_service_set_state(service, SVL_SERVICE_STOPPED);

    return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_stop_signal(svl_service_t *service)
{
    TN_ASSERT(service);
    svl_service_state_t state = svl_service_state(service);
    TN_GUARD(state == SVL_SERVICE_STOPPING || state == SVL_SERVICE_STOPPED || state == SVL_SERVICE_ERROR);
    svl_service_set_state(service, SVL_SERVICE_STOPPING);
    return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
svl_service_state_t svl_service_state(svl_service_t *service)
{
    TN_ASSERT(service);
    return tn_atomic_load(&service->state);
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_events_acquire(svl_service_t *service, tn_event_base_t ***out_evt_base, uint64_t *out_count)
{
    TN_ASSERT(service);
    TN_ASSERT(out_evt_base);
    TN_ASSERT(out_count);

    *out_evt_base = NULL;
    *out_count = service->event_updates_count = 0;
    uint64_t count = service->buffer_count;
    TN_GUARD(tn_event_list_ready_pop_all(&service->events, service->event_updates, &count));

    *out_evt_base = service->event_updates;
    *out_count = service->event_updates_count = count;

    return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_events_release(svl_service_t *service)
{
    TN_ASSERT(service);

    tn_event_client_read_t *evt_read;
    for (size_t i = 0; i < service->event_updates_count; i++) {
        tn_buffer_t *tn_buffer;

        switch (service->event_updates[i]->type) {
        case TN_EVENT_CLIENT_OPEN:
            break;
        case TN_EVENT_CLIENT_CLOSE:
            break;
        case TN_EVENT_CLIENT_READ:
            evt_read = (tn_event_client_read_t *)service->event_updates[i];
            tn_buffer = evt_read->priv;
            tn_buffer_pool_push(&service->pool_read, tn_buffer);
            break;
        default:
            break;
        }
        tn_event_list_free_push(&service->events, service->event_updates[i]);
    }

    return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_send(svl_service_t *service, svl_link_t *link, const uint8_t *sndbuf, const size_t sndlen)
{
    TN_ASSERT(service);
    TN_ASSERT(link);
    TN_ASSERT(sndbuf && sndlen);

    int ret;
    svl_service_write_req_t *send_req;

    /* send req will now be in our hands, but we need to pop it or drop it before return */
    ret = TN_SEND_NOREQ;
    TN_GUARD_CLEANUP(tn_queue_spsc_peek(&service->write_reqs_free, (void **)&send_req));

    ret = TN_SEND_NOBUF;
    TN_GUARD_CLEANUP(tn_buffer_pool_peek(&service->pool_write, &send_req->tn_buffer));
    TN_GUARD_CLEANUP(tn_buffer_write(send_req->tn_buffer, sndbuf, sndlen));

    send_req->svl_link = link;
    send_req->uv_buf.base = tn_buffer_read_ptr(send_req->tn_buffer);
    send_req->uv_buf.len = TN_BUFLEN_CAST(tn_buffer_length(send_req->tn_buffer));

    /* push the request into the ready queue *before* popping anything from free queues */
    ret = TN_SEND_PUSH;
    TN_GUARD(tn_queue_spsc_push(&service->write_reqs_ready, send_req));
    tn_buffer_pool_pop_cached(&service->pool_write);
    tn_queue_spsc_pop_cached(&service->write_reqs_free);

    return TN_SUCCESS;

cleanup:
    tn_log_error("failed to send write request: %d", ret);
    return ret;
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_cmd_open(svl_service_t *service, const tn_endpoint_t *endpoint, uint64_t *cmd_id)
{
    TN_ASSERT(service);
    TN_ASSERT(endpoint);
    TN_ASSERT(cmd_id);

    *cmd_id = UINT64_MAX;

    svl_service_state_t state = svl_service_state(service);
    TN_GUARD_CLEANUP(state != SVL_SERVICE_STARTED);

    tn_cmd_client_open_t *cmd_open;
    TN_GUARD_CLEANUP(tn_cmd_list_free_pop_open(&service->cmds, &cmd_open));
    cmd_open->endpoint = *endpoint;
    *cmd_id = cmd_open->id;
    TN_GUARD_CLEANUP(tn_cmd_list_ready_push(&service->cmds, cmd_open));

    return TN_SUCCESS;

cleanup:
    tn_log_error("error queueing connect");
    return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_cmd_close(svl_service_t *service, const uint64_t client_id, uint64_t *cmd_id)
{
    TN_ASSERT(service);
    TN_ASSERT(cmd_id);

    *cmd_id = UINT64_MAX;

    svl_service_state_t state = svl_service_state(service);
    TN_GUARD_CLEANUP(state != SVL_SERVICE_STARTED);

    tn_cmd_client_close_t *cmd_close;
    TN_GUARD_CLEANUP(tn_cmd_list_free_pop_close(&service->cmds, &cmd_close));
    cmd_close->client_id = client_id;
    *cmd_id = cmd_close->id;
    TN_GUARD_CLEANUP(tn_cmd_list_ready_push(&service->cmds, cmd_close));

    return TN_SUCCESS;

cleanup:
    tn_log_error("error queueing connect");
    return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_cmd_send(svl_service_t *service, const uint64_t client_id, uint64_t *cmd_id, const uint8_t *msg, const size_t msglen)
{
    TN_ASSERT(service);
    TN_ASSERT(cmd_id);
    TN_ASSERT(msg);
    TN_ASSERT(msglen);

    *cmd_id = UINT64_MAX;

    svl_service_state_t state = svl_service_state(service);
    TN_GUARD_CLEANUP(state != SVL_SERVICE_STARTED);

    //tn_cmd_client_send_t *cmd_send;
    //TN_GUARD_CLEANUP(tn_cmd_list_free_pop_send(&service->cmds, &cmd_send));
    //cmd_send->client_id = client_id;
    //cmd_send->buffer = (uint8_t *)msg;
    //cmd_send->len = msglen;
    //*cmd_id = cmd_send->id;
    //TN_GUARD_CLEANUP(tn_cmd_list_ready_push(&service->cmds, cmd_send));

    return TN_SUCCESS;

cleanup:
    tn_log_error("error queueing send");
    return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_stats_clear(svl_service_t *service)
{
    return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int svl_service_stats_get(svl_service_t *service, svl_service_stats_t *stats)
{
    return TN_SUCCESS;
}
