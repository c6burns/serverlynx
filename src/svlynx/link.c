#include "svlynx/link.h"

#include "tn/allocator.h"
#include "tn/buffer.h"
#include "tn/buffer_pool.h"
#include "tn/error.h"

#include "svlynx/service.h"

// --------------------------------------------------------------------------------------------------------------
svl_link_state_t svl_link_state(svl_link_t *link)
{
    TN_ASSERT(link);
    return link->state;
}

// --------------------------------------------------------------------------------------------------------------
svl_link_read_state_t svl_link_read_state(svl_link_t *link)
{
    TN_ASSERT(link);
    return link->read_state;
}

// --------------------------------------------------------------------------------------------------------------
int svl_link_read_header(svl_link_t *link, uint32_t *out_len)
{
    TN_ASSERT(link);
    TN_ASSERT(link->read_buffer);
    TN_GUARD_CLEANUP(tn_buffer_read_be32(link->read_buffer, out_len));
    link->next_payload_len = *out_len;
    link->read_state = SVL_LINK_READ_PAYLOAD;
    return TN_SUCCESS;

cleanup:
    *out_len = 0;
    return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int svl_link_read_payload(svl_link_t *link, tn_buffer_span_t *out_span)
{
    TN_ASSERT(link && link->read_buffer);
    TN_ASSERT(out_span);

    out_span->ptr = tn_buffer_read_ptr(link->read_buffer);
    out_span->len = link->next_payload_len;
    TN_GUARD_CLEANUP(tn_buffer_read_skip(link->read_buffer, out_span->len));
    link->read_state = SVL_LINK_READ_HEADER;
    return TN_SUCCESS;

cleanup:
    out_span->ptr = NULL;
    out_span->len = 0;
    return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int svl_link_buffer_swap(svl_link_t *link)
{
    TN_ASSERT(link && link->read_buffer);

    tn_buffer_t *prev_buffer = link->read_buffer;
    TN_GUARD(tn_buffer_pool_pop_back(&link->service->pool_read, &link->read_buffer));
    if (tn_buffer_read_length(prev_buffer)) {
        TN_GUARD(tn_buffer_write_buffer(link->read_buffer, prev_buffer, 0));
    }

    return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int svl_link_list_setup(svl_link_list_t *list, size_t clients_max)
{
    TN_GUARD_NULL(list);
    memset(list, 0, sizeof(*list));

    TN_GUARD_NULL_CLEANUP(list->client_map = TN_MEM_ACQUIRE(clients_max * sizeof(*list->client_map)));
    memset(list->client_map, 0, clients_max * sizeof(*list->client_map));

    TN_GUARD_CLEANUP(tn_list_ptr_setup(&list->client_list_free, clients_max));
    //TN_GUARD_CLEANUP(tn_list_ptr_setup(&list->client_list_open, clients_max, sizeof(void *)));

    svl_link_t *link;
    for (size_t i = 0; i < clients_max; i++) {
        link = &list->client_map[i];
        link->id = i;
        TN_GUARD_CLEANUP(tn_list_ptr_push_back(&list->client_list_free, link));
    }

    list->clients_max = clients_max;

    return TN_SUCCESS;

cleanup:
    tn_list_ptr_cleanup(&list->client_list_free);
    //tn_list_ptr_cleanup(&list->client_list_open);

    TN_MEM_RELEASE(list->client_map);

    return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int svl_link_list_cleanup(svl_link_list_t *list)
{
    TN_GUARD_NULL(list);

    tn_list_ptr_cleanup(&list->client_list_free);
    //tn_list_ptr_cleanup(&list->client_list_open);

    TN_MEM_RELEASE(list->client_map);

    return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int svl_link_list_open(svl_link_list_t *list, svl_link_t **out_link)
{
    int ret;
    size_t index = 0;

    TN_GUARD_NULL(list);
    TN_GUARD_NULL(out_link);

    TN_GUARD(ret = tn_list_ptr_pop_back(&list->client_list_free, (void **)out_link));
    //TN_GUARD(ret = tn_list_ptr_push_back(&list->client_list_open, out_link, &index));
    //(*out_link)->list_id = index;
    (*out_link)->cmd_open_id = TN_CMD_INVALID;
    (*out_link)->cmd_close_id = TN_CMD_INVALID;

    return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int svl_link_list_close(svl_link_list_t *list, svl_link_t *link)
{
    TN_ASSERT(list);
    TN_ASSERT(link);

    //TN_GUARD(tn_list_ptr_remove(&list->client_list_open, link->list_id));
    TN_GUARD(tn_list_ptr_push_back(&list->client_list_free, link));

    return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int svl_link_list_reset(svl_link_list_t *list)
{
    TN_GUARD_NULL(list);
    memset(list->client_map, 0, list->clients_max * sizeof(*list->client_map));

    tn_list_ptr_clear(&list->client_list_free);
    tn_list_ptr_clear(&list->client_list_open);

    svl_link_t *link;
    for (size_t i = 0; i < list->clients_max; i++) {
        link = &list->client_map[i];
        link->id = i;
        TN_GUARD(tn_list_ptr_push_back(&list->client_list_free, link));
    }

    return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int svl_link_list_get(svl_link_list_t *list, uint64_t client_id, svl_link_t **out_link)
{
    TN_ASSERT(list);
    TN_ASSERT(out_link);
    *out_link = NULL;
    if (client_id >= list->clients_max) return TN_ERROR;
    *out_link = &list->client_map[client_id];
    return TN_SUCCESS;
}
