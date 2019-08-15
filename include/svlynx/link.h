#ifndef SVL_LINK_H
#define SVL_LINK_H

#include <stdint.h>

#include "tn/buffer.h"
#include "tn/endpoint.h"
#include "tn/list_ptr.h"

#include "svlynx/svlynx.h"

// forwards
struct svl_service_s;

typedef enum svl_link_state_e {
    SVL_LINK_NEW,
    SVL_LINK_OPEN,
    SVL_LINK_CLOSING,
    SVL_LINK_CLOSED,
    SVL_LINK_ERROR,
    SVL_LINK_INVALID,
} svl_link_state_t;

typedef enum svl_link_read_state_e {
    SVL_LINK_READ_NONE,
    SVL_LINK_READ_HEADER,
    SVL_LINK_READ_PAYLOAD,
    SVL_LINK_READ_ERROR,
    SVL_LINK_READ_INVALID,
} svl_link_read_state_t;

typedef struct svl_link_s {
    void *priv;
    uint64_t id;
    struct svl_service_s *service;
    tn_buffer_t *read_buffer;
    tn_endpoint_t endpoint_local;
    tn_endpoint_t endpoint_remote;
    int32_t error_code;
    svl_link_state_t state;
    svl_link_read_state_t read_state;
    size_t next_payload_len;
    uint64_t last_msg_id;
    uint64_t cmd_open_id;
    uint64_t cmd_close_id;
} svl_link_t;

typedef struct svl_link_list_s {
    void *priv;
    svl_link_t *client_map;
    size_t clients_max;
    tn_list_ptr_t client_list_free;
    tn_list_ptr_t client_list_open;
} svl_link_list_t;

svl_link_state_t svl_link_state(svl_link_t *link);
svl_link_read_state_t svl_link_read_state(svl_link_t *link);
int svl_link_read_header(svl_link_t *link, uint32_t *out_len);
int svl_link_read_payload(svl_link_t *link, tn_buffer_span_t *out_span);
int svl_link_buffer_swap(svl_link_t *link);

int svl_link_list_setup(svl_link_list_t *list, size_t clients_max);
int svl_link_list_cleanup(svl_link_list_t *list);
int svl_link_list_open(svl_link_list_t *list, svl_link_t **out_link);
int svl_link_list_close(svl_link_list_t *list, svl_link_t *link);
int svl_link_list_reset(svl_link_list_t *list);
int svl_link_list_get(svl_link_list_t *list, uint64_t client_id, svl_link_t **out_link);

#endif
