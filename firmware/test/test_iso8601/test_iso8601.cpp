#include <unity.h>

#include "iso8601.h"

void test_parses_valid_utc_timestamp() {
    int64_t epoch = parseIso8601ToEpoch("2024-03-20T12:34:56Z");
    TEST_ASSERT_EQUAL_INT64(1710938096LL, epoch);
}

void test_returns_negative_one_on_malformed_input() {
    TEST_ASSERT_EQUAL_INT64(-1, parseIso8601ToEpoch("not-a-date"));
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_valid_utc_timestamp);
    RUN_TEST(test_returns_negative_one_on_malformed_input);
    return UNITY_END();
}
