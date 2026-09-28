#include <cstddef>
#include <cstdint>

#include <unity.h>

#include "core/arrival_screen.h"

void test_404_is_not_a_backoff() {
    TEST_ASSERT_TRUE(classifyFetch(404, false) == FetchClass::NotFound);
    TEST_ASSERT_TRUE(classifyFetch(404, true) == FetchClass::NotFound);
}

void test_transport_and_server_errors_back_off() {
    TEST_ASSERT_TRUE(classifyFetch(0, false) == FetchClass::Backoff);
    TEST_ASSERT_TRUE(classifyFetch(-1, false) == FetchClass::Backoff);
    TEST_ASSERT_TRUE(classifyFetch(500, false) == FetchClass::Backoff);
    TEST_ASSERT_TRUE(classifyFetch(503, false) == FetchClass::Backoff);
}

void test_200_without_a_stop_backs_off() {
    TEST_ASSERT_TRUE(classifyFetch(200, false) == FetchClass::Backoff);
}

void test_200_with_a_stop_is_ok() {
    TEST_ASSERT_TRUE(classifyFetch(200, true) == FetchClass::Ok);
}

void test_unset_clock_stays_on_loading() {
    TEST_ASSERT_TRUE(selectArrivalScreen(false, false, true, true, 0) ==
                    ArrivalScreen::Loading);
    TEST_ASSERT_TRUE(selectArrivalScreen(false, true, true, true, 700000) ==
                    ArrivalScreen::Loading);
}

void test_404_screen_does_not_use_the_backoff_message() {
    TEST_ASSERT_TRUE(selectArrivalScreen(true, true, true, true, 0) ==
                    ArrivalScreen::NotFound);
}

void test_ten_minutes_replaces_the_rows() {
    TEST_ASSERT_TRUE(selectArrivalScreen(true, false, true, true, 599999) ==
                    ArrivalScreen::Arrivals);
    TEST_ASSERT_TRUE(selectArrivalScreen(true, false, true, true, 600000) ==
                    ArrivalScreen::NoRecentData);
}

void test_no_more_buses_when_every_row_expired() {
    TEST_ASSERT_TRUE(selectArrivalScreen(true, false, true, false, 1000) ==
                    ArrivalScreen::NoMoreBuses);
}

void test_empty_stop_is_not_no_more_buses() {
    TEST_ASSERT_TRUE(selectArrivalScreen(true, false, false, false, 1000) ==
                    ArrivalScreen::NoServices);
}

void test_backoff_sequence_resets_on_404() {
    BackoffState state;
    TEST_ASSERT_EQUAL_UINT32(60000, backoffIntervalMs(state.step));

    applyFetchOutcome(state, FetchClass::Backoff);
    TEST_ASSERT_EQUAL_UINT32(120000, backoffIntervalMs(state.step));
    applyFetchOutcome(state, FetchClass::Backoff);
    TEST_ASSERT_EQUAL_UINT32(240000, backoffIntervalMs(state.step));
    applyFetchOutcome(state, FetchClass::Backoff);
    TEST_ASSERT_EQUAL_UINT32(300000, backoffIntervalMs(state.step));
    applyFetchOutcome(state, FetchClass::Backoff);
    TEST_ASSERT_EQUAL_UINT32(300000, backoffIntervalMs(state.step));

    applyFetchOutcome(state, FetchClass::NotFound);
    TEST_ASSERT_EQUAL_UINT32(0, state.step);
    TEST_ASSERT_EQUAL_UINT32(60000, backoffIntervalMs(state.step));

    applyFetchOutcome(state, FetchClass::Ok);
    TEST_ASSERT_EQUAL_UINT32(60000, backoffIntervalMs(state.step));
}

void test_wifi_loss_is_not_a_fetch_failure() {
    // A drop is not a fetch outcome, so the step is whatever the last
    // fetch left it at, and a valid cache stays on the arrivals screen.
    BackoffState state;
    incrementBackoff(state);
    TEST_ASSERT_EQUAL_size_t(1, state.step);
    TEST_ASSERT_TRUE(selectArrivalScreen(true, false, true, true, 1000) ==
                    ArrivalScreen::Arrivals);
}

void test_redraw_skips_an_unchanged_fast_loop() {
    TEST_ASSERT_FALSE(shouldRedrawCached(false, 1000, 1000 + 50));
    TEST_ASSERT_TRUE(shouldRedrawCached(true, 1000, 1000 + 50));
    TEST_ASSERT_FALSE(shouldRedrawCached(false, 0, 50000));
    TEST_ASSERT_TRUE(shouldRedrawCached(false, 1000, 1000 + 15000));
}

void test_age_uses_updated_at_when_it_is_older() {
    const int64_t now = 1800000000;
    TEST_ASSERT_EQUAL_UINT32(200000, arrivalDataAgeMs(0, now, now - 200));
    TEST_ASSERT_EQUAL_UINT32(300000, arrivalDataAgeMs(300000, now, now - 100));
    TEST_ASSERT_EQUAL_UINT32(1000, arrivalDataAgeMs(1000, 1000, now - 200));
}

void test_clock_seed_and_ntp_block_only_while_unset() {
    TEST_ASSERT_TRUE(shouldSeedClock(1000, kClockSetEpoch));
    TEST_ASSERT_FALSE(shouldSeedClock(kClockSetEpoch, kClockSetEpoch + 10));
    TEST_ASSERT_FALSE(shouldSeedClock(1000, 1000));
    TEST_ASSERT_TRUE(shouldBlockForNtp(1000));
    TEST_ASSERT_FALSE(shouldBlockForNtp(kClockSetEpoch));
}

void test_stop_index_clamps_when_the_list_shrinks() {
    TEST_ASSERT_EQUAL_size_t(0, clampStopIndex(5, 0));
    TEST_ASSERT_EQUAL_size_t(0, clampStopIndex(5, 3));
    TEST_ASSERT_EQUAL_size_t(2, clampStopIndex(2, 3));
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_404_is_not_a_backoff);
    RUN_TEST(test_transport_and_server_errors_back_off);
    RUN_TEST(test_200_without_a_stop_backs_off);
    RUN_TEST(test_200_with_a_stop_is_ok);
    RUN_TEST(test_unset_clock_stays_on_loading);
    RUN_TEST(test_404_screen_does_not_use_the_backoff_message);
    RUN_TEST(test_ten_minutes_replaces_the_rows);
    RUN_TEST(test_no_more_buses_when_every_row_expired);
    RUN_TEST(test_empty_stop_is_not_no_more_buses);
    RUN_TEST(test_backoff_sequence_resets_on_404);
    RUN_TEST(test_wifi_loss_is_not_a_fetch_failure);
    RUN_TEST(test_redraw_skips_an_unchanged_fast_loop);
    RUN_TEST(test_age_uses_updated_at_when_it_is_older);
    RUN_TEST(test_clock_seed_and_ntp_block_only_while_unset);
    RUN_TEST(test_stop_index_clamps_when_the_list_shrinks);
    return UNITY_END();
}
