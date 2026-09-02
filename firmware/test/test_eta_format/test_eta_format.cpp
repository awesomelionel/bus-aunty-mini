#include <unity.h>

#include "eta_format.h"

void test_missing_eta_shows_placeholder() {
    TEST_ASSERT_EQUAL_STRING("--", formatEtaMinutes(-1, 1000).c_str());
}

void test_rounds_to_nearest_minute() {
    TEST_ASSERT_EQUAL_STRING("2m", formatEtaMinutes(1090, 1000).c_str());
}

void test_under_thirty_seconds_is_due() {
    TEST_ASSERT_EQUAL_STRING("Due", formatEtaMinutes(1010, 1000).c_str());
}

void test_past_arrival_is_due() {
    TEST_ASSERT_EQUAL_STRING("Due", formatEtaMinutes(995, 1000).c_str());
}

void test_over_an_hour_caps_at_sixty_plus() {
    TEST_ASSERT_EQUAL_STRING("60+", formatEtaMinutes(4660, 1000).c_str());
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_missing_eta_shows_placeholder);
    RUN_TEST(test_rounds_to_nearest_minute);
    RUN_TEST(test_under_thirty_seconds_is_due);
    RUN_TEST(test_past_arrival_is_due);
    RUN_TEST(test_over_an_hour_caps_at_sixty_plus);
    return UNITY_END();
}
