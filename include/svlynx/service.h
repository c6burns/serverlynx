#ifndef SVL_SERVICE_H
#define SVL_SERVICE_H

#include <stdint.h>

#include "tn/buffer_pool.h"
#include "tn/endpoint.h"
#include "tn/cmd.h"
#include "tn/event.h"
#include "tn/queue_spsc.h"
#include "tn/thread.h"

#include "svlynx/svlynx.h"
#include "svlynx/link.h"

#define SVL_SERVICE_MAX_RECV 65535

// forwards
struct svl_service_write_req_s;

typedef enum svl_service_state_e {
    SVL_SERVICE_NEW,
    SVL_SERVICE_STARTING,
    SVL_SERVICE_STARTED,
    SVL_SERVICE_STOPPING,
    SVL_SERVICE_STOPPED,
    SVL_SERVICE_ERROR,
    SVL_SERVICE_INVALID,
} svl_service_state_t;

typedef struct svl_service_stats_s {
    uint64_t recv_count;
    uint64_t recv_bytes;
    uint64_t send_count;
    uint64_t send_bytes;
    uint64_t tstamp_last;
} svl_service_stats_t;

typedef struct svl_service_s {
    /* impl specific struct pointer */
    void *priv;

    /* IO thread (calls uv_run, handles read/write, etc) */
    tn_thread_t thread_io;

    /* server's bind address */
    tn_endpoint_t host_listen;

    /* tcp link context (eg. connections) */
    svl_link_list_t link_list;

    /* IO thread's buffer pool for recv */
    tn_buffer_pool_t pool_read;

    /* main thread's buffer pool for send */
    tn_buffer_pool_t pool_write;

    /* context for the main thread to queue events for the IO thread */
    tn_cmd_list_t cmds;

    /* context for the IO thread to queue events for the main thread */
    tn_event_list_t events;

    /* pointer array of events to pull from the event queue */
    tn_event_base_t **event_updates;
    uint64_t event_updates_count;

    /* to how many connections (inbound and outbound) are we bounded */
    size_t client_count_max;

    /* to how many buffers, event updates, are we bounded */
    size_t buffer_count;

    /* write request pool for main thread */
    struct svl_service_write_req_s *write_reqs;

    /* write request free list for IO thread --> main thread */
    tn_queue_spsc_t write_reqs_free;

    /* write request ready list for main thread --> IO thread */
    tn_queue_spsc_t write_reqs_ready;

    /* overall connection statistics - these will be sent in the event queue - don't touch */
    //tn_event_io_stats_t stats;

    /* server state (connecting, connecting, etc) */
    tn_atomic_t state;
} svl_service_t;

int svl_service_setup(svl_service_t *service, size_t max_clients);
void svl_service_cleanup(svl_service_t *service);
int svl_service_listen(svl_service_t *service, tn_endpoint_t *endpoint);
int svl_service_start(svl_service_t *service);
int svl_service_stop(svl_service_t *service);
int svl_service_stop_signal(svl_service_t *service);
svl_service_state_t svl_service_state(svl_service_t *service);
int svl_service_events_acquire(svl_service_t *service, tn_event_base_t ***out_evt_base, uint64_t *out_count);
int svl_service_events_release(svl_service_t *service);
int svl_service_send(svl_service_t *service, svl_link_t *link, const uint8_t *sndbuf, const size_t sndlen);

int svl_service_cmd_open(svl_service_t *service, const tn_endpoint_t *endpoint, uint64_t *cmd_id);
int svl_service_cmd_close(svl_service_t *service, const uint64_t client_id, uint64_t *cmd_id);
int svl_service_cmd_send(svl_service_t *service, const uint64_t client_id, uint64_t *cmd_id, const uint8_t *msg, const size_t msglen);

int svl_service_stats_clear(svl_service_t *service);
int svl_service_stats_get(svl_service_t *service, svl_service_stats_t *stats);

#endif
