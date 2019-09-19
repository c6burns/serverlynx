#include "tn/test_harness.h"
#include "tn/time.h"
#include "tn/thread.h"
#include "svlynx/service.h"

TN_TEST_CASE_BEGIN(svl_service_setup_cleanup)
    svl_service_t service = {0};

    ASSERT_SUCCESS(svl_service_setup(&service, 1000));
    svl_service_cleanup(&service);

    return TN_SUCCESS;
}

TN_TEST_CASE_BEGIN(svl_service_setup_cleanup_twice)
    svl_service_t service = {0};

    ASSERT_SUCCESS(svl_service_setup(&service, 1000));
    svl_service_cleanup(&service);

    ASSERT_SUCCESS(svl_service_setup(&service, 1000));
    svl_service_cleanup(&service);

    return TN_SUCCESS;
}

TN_TEST_CASE_BEGIN(svl_service_start_stop)
    svl_service_t service = {0};

    ASSERT_SUCCESS(svl_service_setup(&service, 1000));
    ASSERT_SUCCESS(svl_service_start(&service));
    tn_thread_sleep_s(1);
    ASSERT_SUCCESS(svl_service_stop(&service));
    svl_service_cleanup(&service);

    return TN_SUCCESS;
}

TN_TEST_CASE_BEGIN(svl_service_start_stop_twice)
    svl_service_t service = {0};

    ASSERT_SUCCESS(svl_service_setup(&service, 1000));
    ASSERT_SUCCESS(svl_service_start(&service));
    tn_thread_sleep_s(1);
    ASSERT_SUCCESS(svl_service_stop(&service));
    svl_service_cleanup(&service);

    ASSERT_SUCCESS(svl_service_setup(&service, 1000));
    ASSERT_SUCCESS(svl_service_start(&service));
    tn_thread_sleep_s(1);
    ASSERT_SUCCESS(svl_service_stop(&service));
    svl_service_cleanup(&service);

    return TN_SUCCESS;
}

TN_TEST_CASE_BEGIN(svl_service_start_stop_immediately)
    svl_service_t service = {0};

    ASSERT_SUCCESS(svl_service_setup(&service, 1000));
    ASSERT_SUCCESS(svl_service_start(&service));
    ASSERT_SUCCESS(svl_service_stop(&service));
    svl_service_cleanup(&service);

    return TN_SUCCESS;
}

TN_TEST_CASE_BEGIN(svl_service_start_stop_immediately_twice)
    svl_service_t service = {0};

    ASSERT_SUCCESS(svl_service_setup(&service, 1000));
    ASSERT_SUCCESS(svl_service_start(&service));
    ASSERT_SUCCESS(svl_service_stop(&service));
    svl_service_cleanup(&service);

    ASSERT_SUCCESS(svl_service_setup(&service, 1000));
    ASSERT_SUCCESS(svl_service_start(&service));
    ASSERT_SUCCESS(svl_service_stop(&service));
    svl_service_cleanup(&service);

    return TN_SUCCESS;
}

TN_TEST_CASE(test_svl_service_setup_cleanup, svl_service_setup_cleanup)
TN_TEST_CASE(test_svl_service_setup_cleanup_twice, svl_service_setup_cleanup_twice)
TN_TEST_CASE(test_svl_service_start_stop, svl_service_start_stop)
TN_TEST_CASE(test_svl_service_start_stop_twice, svl_service_start_stop_twice)
TN_TEST_CASE(test_svl_service_start_stop_immediately, svl_service_start_stop_immediately)
TN_TEST_CASE(test_svl_service_start_stop_immediately_twice, svl_service_start_stop_immediately_twice)
