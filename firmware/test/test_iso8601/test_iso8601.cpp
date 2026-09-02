#include <unity.h>

#include "iso8601.h"

void test_parses_valid_utc_timestamp() {
    int64_t epoch = parseIso8601ToEpoch("2024-03-20T12:34:56Z");
    TEST_ASSERT_EQUAL_INT64(1710938096LL, epoch);
}

void test_returns_negative_one_on_malformed_input() {
    TEST_ASSERT_EQUAL_INT64(-1, parseIso8601ToEpoch("not-a-date"));
}

void test_parses_positive_offset_timestamp() {
    // 2026-09-02T22:18:31+08:00 == 2026-09-02T14:18:31Z
    int64_t epoch = parseIso8601ToEpoch("2026-09-02T22:18:31+08:00");
    TEST_ASSERT_EQUAL_INT64(1788358711LL, epoch);
}

void test_positive_offset_matches_equivalent_z_timestamp() {
    int64_t offsetEpoch = parseIso8601ToEpoch("2026-09-02T22:18:31+08:00");
    int64_t zEpoch = parseIso8601ToEpoch("2026-09-02T14:18:31Z");
    TEST_ASSERT_EQUAL_INT64(zEpoch, offsetEpoch);
}

void test_parses_negative_offset_timestamp() {
    // 2024-03-20T07:04:56-05:30 == 2024-03-20T12:34:56Z == 1710938096
    int64_t epoch = parseIso8601ToEpoch("2024-03-20T07:04:56-05:30");
    TEST_ASSERT_EQUAL_INT64(1710938096LL, epoch);
}

void test_rejects_malformed_offset() {
    TEST_ASSERT_EQUAL_INT64(-1, parseIso8601ToEpoch("2024-03-20T12:34:56+XX:00"));
}

void test_rejects_out_of_range_month() {
    TEST_ASSERT_EQUAL_INT64(-1, parseIso8601ToEpoch("2024-13-20T12:34:56Z"));
}

void test_rejects_out_of_range_hour() {
    TEST_ASSERT_EQUAL_INT64(-1, parseIso8601ToEpoch("2024-03-20T99:34:56Z"));
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_valid_utc_timestamp);
    RUN_TEST(test_returns_negative_one_on_malformed_input);
    RUN_TEST(test_parses_positive_offset_timestamp);
    RUN_TEST(test_positive_offset_matches_equivalent_z_timestamp);
    RUN_TEST(test_parses_negative_offset_timestamp);
    RUN_TEST(test_rejects_malformed_offset);
    RUN_TEST(test_rejects_out_of_range_month);
    RUN_TEST(test_rejects_out_of_range_hour);
    return UNITY_END();
}
