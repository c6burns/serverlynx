/*
 * Copyright (c) 2019 Chris Burns <chris@kitty.city>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define SL_EXPORTS

#include "socklynx/socklynx.h"
#include "svlynx/service.h"
#include "tn/allocator.h"
#include "tn/error.h"
#include "tn/log.h"

SL_API int32_t SL_CALL socklynx_setup(sl_sys_t *sys)
{
    return sl_sys_setup(sys);
}

SL_API int32_t SL_CALL socklynx_cleanup(sl_sys_t *sys)
{
    return sl_sys_cleanup(sys);
}

SL_API int32_t SL_CALL socklynx_socket_nonblocking(sl_sock_t *sock, uint32_t enabled)
{
    if (enabled) return sl_sock_nonblocking_set(sock);
    return sl_sock_blocking_set(sock);
}

SL_API int32_t SL_CALL socklynx_socket_open(sl_sock_t *sock)
{
    SL_GUARD_NULL(sock);
    SL_GUARD(sl_sock_create(sock, SL_SOCK_TYPE_DGRAM, SL_SOCK_PROTO_UDP));
    SL_GUARD(sl_sock_bind(sock));
    return SL_OK;
}

SL_API int32_t SL_CALL socklynx_socket_close(sl_sock_t *sock)
{
    SL_GUARD_NULL(sock);
    return sl_sock_close(sock);
}

SL_API int32_t SL_CALL socklynx_socket_send(sl_sock_t *sock, sl_buf_t *buf, int32_t bufcount, sl_endpoint_t *endpoint)
{
    SL_GUARD_NULL(sock);
    SL_GUARD_NULL(buf);
    SL_GUARD_NULL(endpoint);
    return sl_sock_send(sock, buf, bufcount, endpoint);
}

SL_API int32_t SL_CALL socklynx_socket_recv(sl_sock_t *sock, sl_buf_t *buf, int32_t bufcount, sl_endpoint_t *endpoint)
{
    SL_GUARD_NULL(sock);
    SL_GUARD_NULL(buf);
    SL_GUARD_NULL(endpoint);
    return sl_sock_recv(sock, buf, bufcount, endpoint);
}

SL_API int32_t SL_CALL servlynx_setup(sl_sys_t *sys, svl_service_t **service_ptr)
{
    TN_GUARD_NULL(sys);
    TN_GUARD_NULL(service_ptr);
    if (*service_ptr) return TN_SUCCESS;

    TN_GUARD(sl_sys_setup(sys));

    TN_GUARD_NULL(*service_ptr = TN_MEM_ACQUIRE(sizeof(**service_ptr)));
    return svl_service_setup(*service_ptr, 1000);
}

int service_state_wait(svl_service_t *service, svl_service_state_t desired_state, int wait_ms)
{
    int total_wait_ms = 0;
    uint64_t ms50 = tn_tstamp_convert(50, TN_TSTAMP_MS, TN_TSTAMP_NS);

    svl_service_state_t state = svl_service_state(service);
    while (state != desired_state) {
        tn_thread_sleep(ms50);
        state = svl_service_state(service);

        total_wait_ms += 50;
        if (total_wait_ms >= wait_ms) return TN_ERROR;
    }

    return TN_SUCCESS;
}

int service_stop(svl_service_t *service)
{
    TN_GUARD_NULL(service);

    svl_service_state_t state = svl_service_state(service);
    switch (state) {
    case SVL_SERVICE_STARTING:
        TN_GUARD(service_state_wait(service, SVL_SERVICE_STARTED, 500));
    case SVL_SERVICE_STARTED:
        TN_GUARD(svl_service_stop(service));
    case SVL_SERVICE_NEW:
    case SVL_SERVICE_STOPPING:
    case SVL_SERVICE_STOPPED:
        break;
    case SVL_SERVICE_ERROR:
    case SVL_SERVICE_INVALID:
        return TN_ERROR;
    }

    TN_GUARD(service_state_wait(service, SVL_SERVICE_STOPPED, 500));

    return TN_SUCCESS;
}

SL_API int32_t SL_CALL servlynx_cleanup(sl_sys_t *sys, svl_service_t **service_ptr)
{
    if (service_ptr) {
        if (service_stop(*service_ptr)) {
            tn_log_error("service_stop failed");
        }
        TN_MEM_RELEASE(*service_ptr);
        *service_ptr = NULL;
    }

    if (sys) {
        if (sl_sys_cleanup(sys)) {
            tn_log_error("sl_sys_cleanup failed");
        }
    }

    return TN_SUCCESS;
}

SL_API int32_t SL_CALL servlynx_start(svl_service_t *service, tn_endpoint_t *endpoint)
{
    TN_GUARD_NULL(service);
    if (endpoint) {
        //TN_GUARD(!tn_endpoint_equal_addr(&service->host_listen, endpoint));
        TN_GUARD(svl_service_listen(service, endpoint));
    }
    return svl_service_start(service);
}

SL_API int32_t SL_CALL servlynx_stop(svl_service_t *service)
{
    TN_GUARD_NULL(service);
    return service_stop(service);
}
