#include <unity.h>

#include "core/eta_format.h"

void test_missing_eta_shows_placeholder() {
    TEST_ASSERT_EQUAL_STRING("--", formatEtaMinutes(-1, 1000).c_str());
}

void test_rounds_down_to_whole_minutes() {
    // 1m30s away, so 1 rather than 2.
    TEST_ASSERT_EQUAL_STRING("1", formatEtaMinutes(1090, 1000).c_str());
}

void test_two_and_a_half_minutes_rounds_down_to_two() {
    TEST_ASSERT_EQUAL_STRING("2", formatEtaMinutes(1150, 1000).c_str());
}

void test_just_under_a_minute_still_shows_the_lower_minute() {
    TEST_ASSERT_EQUAL_STRING("2", formatEtaMinutes(1179, 1000).c_str());
    TEST_ASSERT_EQUAL_STRING("3", formatEtaMinutes(1180, 1000).c_str());
}

void test_exactly_sixty_minutes_is_not_capped() {
    TEST_ASSERT_EQUAL_STRING("60", formatEtaMinutes(4600, 1000).c_str());
}

void test_under_thirty_seconds_is_arriving() {
    TEST_ASSERT_EQUAL_STRING("Arr", formatEtaMinutes(1010, 1000).c_str());
}

void test_past_arrival_is_arriving() {
    TEST_ASSERT_EQUAL_STRING("Arr", formatEtaMinutes(995, 1000).c_str());
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
    RUN_TEST(test_two_and_a_half_minutes_rounds_down_to_two);
    RUN_TEST(test_just_under_a_minute_still_shows_the_lower_minute);
    RUN_TEST(test_exactly_sixty_minutes_is_not_capped);
    RUN_TEST(test_under_thirty_seconds_is_arriving);
    RUN_TEST(test_past_arrival_is_arriving);
    RUN_TEST(test_over_an_hour_caps_at_sixty_plus);
    return UNITY_END();
}
