#include <unity.h>

#include "core/eta_format.h"

void test_missing_eta_shows_placeholder() {
    TEST_ASSERT_EQUAL_STRING("--", formatEtaMinutes(-1, 1000).c_str());
}

void test_rounds_down_to_whole_minutes() {
    // 3m30s away, so 3 rather than 4.
    TEST_ASSERT_EQUAL_STRING("3", formatEtaMinutes(1210, 1000).c_str());
}

void test_crossing_a_minute_boundary() {
    TEST_ASSERT_EQUAL_STRING("2", formatEtaMinutes(1179, 1000).c_str());
    TEST_ASSERT_EQUAL_STRING("3", formatEtaMinutes(1180, 1000).c_str());
}

void test_exactly_sixty_minutes_is_not_capped() {
    TEST_ASSERT_EQUAL_STRING("60", formatEtaMinutes(4600, 1000).c_str());
}

void test_arriving_window_covers_two_minutes_ahead() {
    TEST_ASSERT_EQUAL_STRING("Arr", formatEtaMinutes(1120, 1000).c_str());
    TEST_ASSERT_EQUAL_STRING("Arr", formatEtaMinutes(1010, 1000).c_str());
}

void test_just_outside_the_window_shows_minutes_again() {
    TEST_ASSERT_EQUAL_STRING("2", formatEtaMinutes(1121, 1000).c_str());
}

void test_any_past_arrival_keeps_showing_arriving() {
    TEST_ASSERT_EQUAL_STRING("Arr", formatEtaMinutes(995, 1000).c_str());
    TEST_ASSERT_EQUAL_STRING("Arr", formatEtaMinutes(880, 1000).c_str());
    TEST_ASSERT_EQUAL_STRING("Arr", formatEtaMinutes(1, 1000).c_str());
}

void test_over_an_hour_caps_at_sixty_plus() {
    TEST_ASSERT_EQUAL_STRING("60+", formatEtaMinutes(4660, 1000).c_str());
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_missing_eta_shows_placeholder);
    RUN_TEST(test_rounds_down_to_whole_minutes);
    RUN_TEST(test_crossing_a_minute_boundary);
    RUN_TEST(test_exactly_sixty_minutes_is_not_capped);
    RUN_TEST(test_arriving_window_covers_two_minutes_ahead);
    RUN_TEST(test_just_outside_the_window_shows_minutes_again);
    RUN_TEST(test_any_past_arrival_keeps_showing_arriving);
    RUN_TEST(test_over_an_hour_caps_at_sixty_plus);
    return UNITY_END();
}
