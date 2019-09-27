#include "svlynx_lg/svlynx_lg.h"

#include "tn/error.h"
#include "tn/event.h"
#include "tn/log.h"
#include "tn/queue_spsc.h"
#include "tn/system.h"

#include "svlynx/link.h"
#include "svlynx/service.h"

uint64_t cmd_open_id = TN_CMD_INVALID;
uint64_t cmd_close_id = TN_CMD_INVALID;
uint64_t cmd_read_id = TN_CMD_INVALID;
uint64_t client_id = TN_CMD_INVALID;

int handle_client_read(svl_service_t *service, tn_event_client_read_t *evt_read)
{
    int ret;
    svl_link_t *link;

    ret = TN_EVENT_NOCHAN;
    TN_GUARD_CLEANUP(svl_link_list_get(&service->link_list, evt_read->client_id, &link));
    assert(link);

    // tn_buffer_t *tn_buffer = evt_read->priv;
    // tn_buffer_read_reset(tn_buffer);
    // TN_GUARD_CLEANUP(ret = svl_service_send(service, link, tn_buffer_read_ptr(tn_buffer), tn_buffer_read_length(tn_buffer)));

    return TN_SUCCESS;

cleanup:
    if (ret) tn_log_error("failed to process read event: %d", ret);
    return ret;
}

void handle_client_open(svl_service_t *service, tn_event_client_open_t *evt)
{
    char ipstr[255];
    uint16_t port;
    tn_endpoint_string_get(&evt->host, &port, ipstr, sizeof(ipstr));
    if (cmd_open_id == evt->cmd_id) {
        tn_log_info("TN_EVENT_CLIENT_OPEN (client connect): %s:%u -- %llu", ipstr, port, evt->cmd_id);
        client_id = evt->client_id;
    } else {
        tn_log_info("TN_EVENT_CLIENT_OPEN (server accept): %s:%u -- %llu", ipstr, port, evt->cmd_id);
    }
}

void handle_client_close(svl_service_t *service, tn_event_client_close_t *evt)
{
    if (cmd_close_id == evt->cmd_id) {
        tn_log_info("TN_EVENT_CLIENT_CLOSE (client close): %llu", evt->client_id);
    } else {
        tn_log_info("TN_EVENT_CLIENT_CLOSE (server close): %llu", evt->client_id);
    }
}

int main(void)
{
    int ret;
    uint32_t frame = 0;
    tn_system_t system;
    tn_endpoint_t ep_connect = {0};
    tn_endpoint_t ep_listen = {0};
    svl_service_t svl_service = {
        .priv = NULL,
    };

    size_t num_clients = 1;
    double num_seconds = 10.0;
    const uint8_t *msg = "This is my network message :D\0";
    const size_t msglen = strlen(msg) + 1;

    TN_GUARD(tn_system_setup(&system));

    uint32_t cores = tn_system_cpu_count(&system);
    tn_thread_set_workers(cores);

    tn_endpoint_from_byte(&ep_connect, 7777, 127, 0, 0, 1);

    TN_GUARD(svl_service_setup(&svl_service, SVL_LG_MAX_CLIENTS));
    TN_GUARD(svl_service_start(&svl_service));

    tn_thread_sleep(tn_tstamp_convert(1, TN_TSTAMP_S, TN_TSTAMP_NS));

    while (svl_service_state(&svl_service) == SVL_SERVICE_STARTED) {
        // emulate 60 fps tick rate on Unity main thread
        tn_thread_sleep_ms(16);

        tn_event_base_t **evt_base;
        uint64_t evt_count = 0;
        svl_service_events_acquire(&svl_service, &evt_base, &evt_count);

        for (int eidx = 0; eidx < evt_count; eidx++) {
            switch (evt_base[eidx]->type) {
            case TN_EVENT_CLIENT_OPEN:
                handle_client_open(&svl_service, (tn_event_client_open_t *)evt_base[eidx]);
                break;
            case TN_EVENT_CLIENT_CLOSE:
                handle_client_close(&svl_service, (tn_event_client_close_t *)evt_base[eidx]);
                break;
            case TN_EVENT_CLIENT_READ:
                ret = handle_client_read(&svl_service, (tn_event_client_read_t *)evt_base[eidx]);
                break;
            default:
                break;
            }
        }

        svl_service_events_release(&svl_service);

        //double elapsed_d = (double)tn_tstamp() / (double)TN_TIME_NS_PER_S;
        //if (elapsed_d > num_seconds) {
        //    break;
        //}

        if (frame == 0) {
            if (svl_service_cmd_open(&svl_service, &ep_connect, &cmd_open_id)) {
                tn_log_error("connect failed");
            } else {
                tn_log_info("connecting with cmd_id: %llu", cmd_open_id);
            }
        } else if ((frame & 63) == 0 && client_id != TN_CMD_INVALID) {
            if (svl_service_cmd_send(&svl_service, client_id, &cmd_read_id, msg, msglen)) {
                tn_log_error("send failed");
            } else {
                tn_log_info("send with cmd_id: &llu", cmd_read_id);
            }
        }

        frame++;
    }

    TN_GUARD(svl_service_stop(&svl_service));
    svl_service_cleanup(&svl_service);
    tn_system_cleanup(&system);

    return 0;
}
