#include "tn/test_harness.h"
#include "tn/time.h"
#include "tn/thread.h"
#include "plugin.c"

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

TN_TEST_CASE(test_svl_plugin_setup_cleanup, svl_plugin_setup_cleanup)
TN_TEST_CASE(test_svl_plugin_setup_cleanup_twice, svl_plugin_setup_cleanup_twice)
TN_TEST_CASE(test_svl_plugin_start_stop, svl_plugin_start_stop)
TN_TEST_CASE(test_svl_plugin_start_stop_twice, svl_plugin_start_stop_twice)
TN_TEST_CASE(test_svl_plugin_start_stop_immediately, svl_plugin_start_stop_immediately)
TN_TEST_CASE(test_svl_plugin_start_stop_immediately_twice, svl_plugin_start_stop_immediately_twice)
