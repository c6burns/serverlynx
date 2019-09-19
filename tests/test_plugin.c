#include "tn/error.h"
#if TN_PLATFORM_WINDOWS
#    include <winsock2.h>
#    define SVL_SONAME "svlynxDSO.dll"
#elif TN_PLATFORM_POSIX
#    define SVL_SONAME "./libsvlynxDSO.so"
#    elif TN_PLATFORM_OSX
#    define SVL_SONAME "svlynxDSO.dylib"
#else
#    error "Invalid platform"
#endif

#include "tn/test_harness.h"
#include "tn/dso.h"
#include "socklynx/sys.h"
#include "svlynx/service.h"

#define SVL_DSO_NAME_SETUP "servlynx_setup"
#define SVL_DSO_NAME_CLEANUP "servlynx_cleanup"
#define SVL_DSO_NAME_START "servlynx_start"
#define SVL_DSO_NAME_STOP "servlynx_stop"

typedef int (*svl_dso_servlynx_setup)(sl_sys_t *sys, svl_service_t **service_ptr);
typedef int (*svl_dso_servlynx_cleanup)(sl_sys_t *sys, svl_service_t **service_ptr);
typedef int (*svl_dso_servlynx_start)(svl_service_t *service, tn_endpoint_t *endpoint);
typedef int (*svl_dso_servlynx_stop)(svl_service_t *service);

tn_dso_t dso = {0};
static svl_dso_servlynx_setup servlynx_setup = NULL;
static svl_dso_servlynx_cleanup servlynx_cleanup = NULL;
static svl_dso_servlynx_start servlynx_start = NULL;
static svl_dso_servlynx_stop servlynx_stop = NULL;

TN_TEST_FIXTURE_DECL(svl_dso_setup)
    memset(&dso, 0, sizeof(dso));
    TN_ASSERT(TN_SUCCESS == tn_dso_setup(&dso, SVL_SONAME));
    TN_ASSERT(TN_SUCCESS == tn_dso_symbol(&dso, SVL_DSO_NAME_SETUP, (void **)&servlynx_setup));
    TN_ASSERT(TN_SUCCESS == tn_dso_symbol(&dso, SVL_DSO_NAME_CLEANUP, (void **)&servlynx_cleanup));
    TN_ASSERT(TN_SUCCESS == tn_dso_symbol(&dso, SVL_DSO_NAME_START, (void **)&servlynx_start));
    TN_ASSERT(TN_SUCCESS == tn_dso_symbol(&dso, SVL_DSO_NAME_STOP, (void **)&servlynx_stop));
}

TN_TEST_FIXTURE_DECL(svl_dso_cleanup)
    TN_ASSERT(TN_SUCCESS == tn_dso_cleanup(&dso));
}

TN_TEST_CASE_BEGIN(svl_plugin_setup_cleanup)
    sl_sys_t sys;
    svl_service_t *service = NULL;

    ASSERT_SUCCESS(servlynx_setup(&sys, &service));
    ASSERT_SUCCESS(servlynx_cleanup(&sys, &service));

    return TN_SUCCESS;
}

TN_TEST_CASE_BEGIN(svl_plugin_setup_cleanup_twice)
    sl_sys_t sys;
    svl_service_t *service = NULL;

    ASSERT_SUCCESS(servlynx_setup(&sys, &service));
    ASSERT_SUCCESS(servlynx_cleanup(&sys, &service));

    ASSERT_SUCCESS(servlynx_setup(&sys, &service));
    ASSERT_SUCCESS(servlynx_cleanup(&sys, &service));

    return TN_SUCCESS;
}

TN_TEST_CASE_BEGIN(svl_plugin_start_stop)
    sl_sys_t sys;
    svl_service_t *service = NULL;

    ASSERT_SUCCESS(servlynx_setup(&sys, &service));
    ASSERT_SUCCESS(servlynx_start(service, NULL));
    tn_thread_sleep_s(1);
    ASSERT_SUCCESS(servlynx_stop(service));
    ASSERT_SUCCESS(servlynx_cleanup(&sys, &service));

    return TN_SUCCESS;
}

TN_TEST_CASE_BEGIN(svl_plugin_start_stop_twice)
    sl_sys_t sys;
    svl_service_t *service = NULL;

    ASSERT_SUCCESS(servlynx_setup(&sys, &service));
    ASSERT_SUCCESS(servlynx_start(service, NULL));
    tn_thread_sleep_s(1);
    ASSERT_SUCCESS(servlynx_stop(service));
    ASSERT_SUCCESS(servlynx_cleanup(&sys, &service));

    ASSERT_SUCCESS(servlynx_setup(&sys, &service));
    ASSERT_SUCCESS(servlynx_start(service, NULL));
    tn_thread_sleep_s(1);
    ASSERT_SUCCESS(servlynx_stop(service));
    ASSERT_SUCCESS(servlynx_cleanup(&sys, &service));

    return TN_SUCCESS;
}

TN_TEST_CASE_BEGIN(svl_plugin_start_stop_immediately)
    sl_sys_t sys;
    svl_service_t *service = NULL;

    ASSERT_SUCCESS(servlynx_setup(&sys, &service));
    ASSERT_SUCCESS(servlynx_start(service, NULL));
    ASSERT_SUCCESS(servlynx_stop(service));
    ASSERT_SUCCESS(servlynx_cleanup(&sys, &service));

    return TN_SUCCESS;
}

TN_TEST_CASE_BEGIN(svl_plugin_start_stop_immediately_twice)
    sl_sys_t sys;
    svl_service_t *service = NULL;

    ASSERT_SUCCESS(servlynx_setup(&sys, &service));
    ASSERT_SUCCESS(servlynx_start(service, NULL));
    ASSERT_SUCCESS(servlynx_stop(service));
    ASSERT_SUCCESS(servlynx_cleanup(&sys, &service));

    ASSERT_SUCCESS(servlynx_setup(&sys, &service));
    ASSERT_SUCCESS(servlynx_start(service, NULL));
    ASSERT_SUCCESS(servlynx_stop(service));
    ASSERT_SUCCESS(servlynx_cleanup(&sys, &service));

    return TN_SUCCESS;
}

TN_TEST_CASE_FIXTURE(test_svl_plugin_setup_cleanup, svl_dso_setup, svl_plugin_setup_cleanup, svl_dso_cleanup, NULL)
TN_TEST_CASE_FIXTURE(test_svl_plugin_setup_cleanup_twice, svl_dso_setup, svl_plugin_setup_cleanup_twice, svl_dso_cleanup, NULL)
TN_TEST_CASE_FIXTURE(test_svl_plugin_start_stop, svl_dso_setup, svl_plugin_start_stop, svl_dso_cleanup, NULL)
TN_TEST_CASE_FIXTURE(test_svl_plugin_start_stop_twice, svl_dso_setup, svl_plugin_start_stop_twice, svl_dso_cleanup, NULL)
TN_TEST_CASE_FIXTURE(test_svl_plugin_start_stop_immediately, svl_dso_setup, svl_plugin_start_stop_immediately, svl_dso_cleanup, NULL)
TN_TEST_CASE_FIXTURE(test_svl_plugin_start_stop_immediately_twice, svl_dso_setup, svl_plugin_start_stop_immediately_twice, svl_dso_cleanup, NULL)
