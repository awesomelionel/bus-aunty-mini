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

void test_wifi_loss_keeps_a_valid_cache() {
    TEST_ASSERT_TRUE(keepArrivalsOnWifiLoss(true));
    TEST_ASSERT_FALSE(keepArrivalsOnWifiLoss(false));
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
    RUN_TEST(test_wifi_loss_keeps_a_valid_cache);
    return UNITY_END();
}
