#include "tn/test_harness.h"
#include "tn/log.h"
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

#define TN_TEST_ELAPSED ((tn_tstamp() - elapsed_base) > elapsed_max)
TN_TEST_CASE_BEGIN(svl_service_listen_acquire)
    svl_service_t service[2] = {0};
    tn_endpoint_t ep_loopback;
    tn_list_ptr_t client_list[2] = {0};
    int client_open[2] = {0};
    int client_close[2] = {0};
    int client_read[2] = {0};
    int i;
    const char *payload = "test message ;) ;)\0";
    size_t payload_len = strlen(payload) + 1;
    uint64_t elapsed_max = tn_tstamp_convert(10, TN_TSTAMP_S, TN_TSTAMP_NS);
    uint64_t elapsed_base = tn_tstamp();
    int max_clients = 10;

    typedef enum test_state_e {
        TEST_STATE_NONE,
        TEST_STATE_CONNECTING,
        TEST_STATE_CONNECTED,
        TEST_STATE_SENDING,
        TEST_STATE_SENT,
        TEST_STATE_DISCONNECTING,
        TEST_STATE_DISCONNECTED,
        TEST_STATE_COMPLETE,
    } test_state_t;

    tn_endpoint_from_byte(&ep_loopback, 54854, 127, 0, 0, 1);

    for (i = 0; i < 2; i++) {
        ASSERT_SUCCESS(tn_list_ptr_setup(&client_list[i], max_clients));
        ASSERT_SUCCESS(svl_service_setup(&service[i], 10));
        if (i == 0) ASSERT_SUCCESS(svl_service_listen(&service[i], &ep_loopback));
        ASSERT_SUCCESS(svl_service_start(&service[i]));
        while (svl_service_state(&service[i]) != SVL_SERVICE_STARTED) {
            tn_thread_sleep_ms(10);
            TN_GUARD(TN_TEST_ELAPSED);
        }
    }

    test_state_t test_state = TEST_STATE_NONE;

    while (test_state != TEST_STATE_COMPLETE) {
        TN_GUARD(TN_TEST_ELAPSED);

        for (i = 0; i < 2; i++) {
            tn_event_base_t **evt_base;
            uint64_t evt_count = 0;
            svl_service_events_acquire(&service[i], &evt_base, &evt_count);

            for (int eidx = 0; eidx < evt_count; eidx++) {
                switch (evt_base[eidx]->type) {
                case TN_EVENT_CLIENT_OPEN:
                {
                    tn_event_client_open_t *evt = (tn_event_client_open_t *)evt_base[eidx];
                    tn_log_trace("TN_EVENT_CLIENT_OPEN: %d -- client_id: %llu", i, evt->client_id);
                    client_open[i]++;
                    tn_list_ptr_push_back(&client_list[i], (void *)(uintptr_t)((tn_event_client_open_t *)evt_base[eidx])->client_id);
                    break;
                }
                case TN_EVENT_CLIENT_CLOSE:
                    tn_log_trace("TN_EVENT_CLIENT_CLOSE: %d", i);
                    client_close[i]++;
                    break;
                case TN_EVENT_CLIENT_READ:
                    tn_log_trace("TN_EVENT_CLIENT_READ: %d", i);
                    client_read[i]++;
                    break;
                default:
                    break;
                }
            }

            svl_service_events_release(&service[i]);
        }

        if (test_state == TEST_STATE_NONE) {
            test_state++;
            tn_log_trace("Entering state: TEST_STATE_CONNECTING");
            uint64_t cmd_id = 0;
            ASSERT_SUCCESS(svl_service_cmd_open(&service[1], &ep_loopback, &cmd_id));
        } else if (test_state == TEST_STATE_CONNECTING) {
            if (client_open[0] && client_open[1]) {
                test_state++;
                tn_log_trace("Entering state: TEST_STATE_CONNECTED");
            }
        } else if (test_state == TEST_STATE_CONNECTED) {
            test_state++;
            tn_log_trace("Entering state: TEST_STATE_SENDING");

            uint64_t cmd_id = 0;
            uint64_t client_id = 0;

            client_id = (uint64_t)(uintptr_t)tn_list_ptr_get(&client_list[0], 0);
            ASSERT_SUCCESS(svl_service_cmd_send(&service[0], client_id, &cmd_id, (const uint8_t *)payload, payload_len));

            client_id = (uint64_t)(uintptr_t)tn_list_ptr_get(&client_list[1], 0);
            ASSERT_SUCCESS(svl_service_cmd_send(&service[1], client_id, &cmd_id, (const uint8_t *)payload, payload_len));
        } else if (test_state == TEST_STATE_SENDING) {
            if (client_read[0] && client_read[1]) {
                test_state++;
                tn_log_trace("Entering state: TEST_STATE_SENT");
            }
        } else if (test_state == TEST_STATE_SENT) {
            test_state++;
            tn_log_trace("Entering state: TEST_STATE_DISCONNECTING");

            uint64_t cmd_id = 0;
            uint64_t client_id = 0;

            client_id = (uint64_t)(uintptr_t)tn_list_ptr_get(&client_list[0], 0);
            ASSERT_SUCCESS(svl_service_cmd_close(&service[0], client_id, &cmd_id));

            client_id = (uint64_t)(uintptr_t)tn_list_ptr_get(&client_list[1], 0);
            ASSERT_SUCCESS(svl_service_cmd_close(&service[1], client_id, &cmd_id));
        } else if (test_state == TEST_STATE_DISCONNECTING) {
            if (client_close[0] && client_close[1]) {
                test_state++;
                tn_log_trace("Entering state: TEST_STATE_DISCONNECTED");
            }
        } else if (test_state == TEST_STATE_DISCONNECTED) {
            test_state++;
            tn_log_trace("Entering state: TEST_STATE_COMPLETED");
        }
    }

    for (i = 0; i < 2; i++) {
        tn_list_ptr_cleanup(&client_list[i]);
        ASSERT_SUCCESS(svl_service_stop(&service[i]));
        svl_service_cleanup(&service[i]);
    }

	return TN_SUCCESS;
}

TN_TEST_CASE(svl_service_setup_cleanup);
TN_TEST_CASE(svl_service_setup_cleanup_twice);
TN_TEST_CASE(svl_service_start_stop);
TN_TEST_CASE(svl_service_start_stop_twice);
TN_TEST_CASE(svl_service_start_stop_immediately);
TN_TEST_CASE(svl_service_start_stop_immediately_twice);
TN_TEST_CASE(svl_service_listen_acquire);
