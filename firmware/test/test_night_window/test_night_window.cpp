#include <unity.h>

#include "core/night_window.h"

namespace {

// 2026-09-22 00:00:00 UTC, a Tuesday. Every case below is an offset from this,
// so the local times in the test names are SGT (UTC+8) unless said otherwise.
//
// Worth checking against `date -u -r` if you change it: because the constant
// sits on both sides of every comparison here, a wrong value still passes
// every test while quietly describing the wrong day.
constexpr int64_t kMidnightUtc = 1790035200;

// Epoch for a given SGT wall-clock hour on that day.
int64_t sgt(int hour, int minute = 0) {
    return kMidnightUtc - kLocalUtcOffsetSeconds + hour * 3600 + minute * 60;
}

bool nightAtSgt(int hour, int minute = 0) {
    return isNightAt(sgt(hour, minute), kLocalUtcOffsetSeconds);
}

}  // namespace

void test_the_offset_is_applied() {
    // Midnight UTC is 08:00 in Singapore, not midnight.
    TEST_ASSERT_EQUAL_INT(8, localHourFor(kMidnightUtc,
                                          kLocalUtcOffsetSeconds));
    TEST_ASSERT_EQUAL_INT(0, localHourFor(kMidnightUtc, 0));
}

void test_daytime_is_not_night() {
    TEST_ASSERT_FALSE(nightAtSgt(8));
    TEST_ASSERT_FALSE(nightAtSgt(12));
    TEST_ASSERT_FALSE(nightAtSgt(18, 59));
}

void test_night_starts_at_nineteen_hundred() {
    TEST_ASSERT_FALSE(nightAtSgt(18, 59));
    TEST_ASSERT_TRUE(nightAtSgt(19, 0));
    TEST_ASSERT_TRUE(nightAtSgt(23, 59));
}

// The window wraps midnight, which is the case a single comparison gets wrong.
void test_night_continues_past_midnight() {
    TEST_ASSERT_TRUE(nightAtSgt(0, 0));
    TEST_ASSERT_TRUE(nightAtSgt(3));
    TEST_ASSERT_TRUE(nightAtSgt(6, 59));
}

void test_day_resumes_at_seven() {
    TEST_ASSERT_TRUE(nightAtSgt(6, 59));
    TEST_ASSERT_FALSE(nightAtSgt(7, 0));
}

// Before NTP lands, time(nullptr) returns something near zero. That must not
// read as the small hours and put a fresh boot into the dark palette.
void test_an_unset_clock_reads_as_day() {
    TEST_ASSERT_EQUAL_INT(-1, localHourFor(0, kLocalUtcOffsetSeconds));
    TEST_ASSERT_FALSE(isNightAt(0, kLocalUtcOffsetSeconds));
    TEST_ASSERT_FALSE(isNightAt(kClockSetEpoch - 1, kLocalUtcOffsetSeconds));
}

void test_the_clock_counts_as_set_at_the_threshold() {
    TEST_ASSERT_NOT_EQUAL(-1, localHourFor(kClockSetEpoch,
                                           kLocalUtcOffsetSeconds));
}

void test_every_hour_is_either_day_or_night() {
    int nightHours = 0;
    for (int hour = 0; hour < 24; ++hour) {
        if (nightAtSgt(hour)) {
            ++nightHours;
        }
    }
    // 19:00-07:00 is twelve hours of the twenty-four.
    TEST_ASSERT_EQUAL_INT(12, nightHours);
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_the_offset_is_applied);
    RUN_TEST(test_daytime_is_not_night);
    RUN_TEST(test_night_starts_at_nineteen_hundred);
    RUN_TEST(test_night_continues_past_midnight);
    RUN_TEST(test_day_resumes_at_seven);
    RUN_TEST(test_an_unset_clock_reads_as_day);
    RUN_TEST(test_the_clock_counts_as_set_at_the_threshold);
    RUN_TEST(test_every_hour_is_either_day_or_night);
    return UNITY_END();
}
