#include <unity.h>

#include "core/backoff_scheduler.h"

void test_first_attempt_returns_true() {
    BackoffState state;
    TEST_ASSERT_TRUE(shouldAttemptFetch(state, 1000));
}

void test_backoff_intervals() {
    TEST_ASSERT_EQUAL_UINT32(60000, backoffIntervalMs(0));
    TEST_ASSERT_EQUAL_UINT32(120000, backoffIntervalMs(1));
    TEST_ASSERT_EQUAL_UINT32(240000, backoffIntervalMs(2));
    TEST_ASSERT_EQUAL_UINT32(300000, backoffIntervalMs(3));
    TEST_ASSERT_EQUAL_UINT32(300000, backoffIntervalMs(4));
    TEST_ASSERT_EQUAL_UINT32(300000, backoffIntervalMs(5));  // Caps at last
}

void test_backoff_increments_and_caps() {
    BackoffState state;
    TEST_ASSERT_EQUAL_UINT32(0, state.step);
    
    incrementBackoff(state);
    TEST_ASSERT_EQUAL_UINT32(1, state.step);
    
    incrementBackoff(state);
    TEST_ASSERT_EQUAL_UINT32(2, state.step);
    
    incrementBackoff(state);
    TEST_ASSERT_EQUAL_UINT32(3, state.step);
    
    incrementBackoff(state);
    TEST_ASSERT_EQUAL_UINT32(4, state.step);
    
    incrementBackoff(state);
    TEST_ASSERT_EQUAL_UINT32(4, state.step);  // Capped at 4
}

void test_backoff_resets() {
    BackoffState state;
    state.step = 3;
    resetBackoff(state);
    TEST_ASSERT_EQUAL_UINT32(0, state.step);
}

void test_should_attempt_after_interval() {
    BackoffState state;
    state.step = 0;
    state.lastAttemptMs = 1000;
    
    // Before interval
    TEST_ASSERT_FALSE(shouldAttemptFetch(state, 1000 + 59999));
    
    // At interval
    TEST_ASSERT_TRUE(shouldAttemptFetch(state, 1000 + 60000));
    
    // After interval
    TEST_ASSERT_TRUE(shouldAttemptFetch(state, 1000 + 70000));
}

void test_data_age_wrap_safe() {
    // Normal case
    TEST_ASSERT_EQUAL_UINT32(5000, dataAgeMs(1000, 6000));
    
    // Wrap case: millis() wrapped from UINT32_MAX to small value
    uint32_t before = UINT32_MAX - 1000;
    uint32_t after = 1000;
    uint32_t expected = after - before;  // Unsigned arithmetic wraps correctly
    TEST_ASSERT_EQUAL_UINT32(expected, dataAgeMs(before, after));
}

void test_data_stale_threshold() {
    uint32_t timestamp = 1000;
    
    // Fresh data
    TEST_ASSERT_FALSE(isDataStale(timestamp, timestamp + 599999));
    
    // At threshold
    TEST_ASSERT_TRUE(isDataStale(timestamp, timestamp + 600000));
    
    // Stale
    TEST_ASSERT_TRUE(isDataStale(timestamp, timestamp + 700000));
}

void test_data_stale_with_wrap() {
    // Data timestamp near UINT32_MAX
    uint32_t timestamp = UINT32_MAX - 100000;
    uint32_t nowWrapped = 500000;  // Wrapped around
    
    // Age = nowWrapped - timestamp (wraps correctly)
    uint32_t age = nowWrapped - timestamp;
    TEST_ASSERT_TRUE(age >= kStaleDataThresholdMs);
    TEST_ASSERT_TRUE(isDataStale(timestamp, nowWrapped));
}

void test_should_refresh_display() {
    uint32_t lastRefresh = 1000;
    
    // Initial state
    TEST_ASSERT_FALSE(shouldRefreshDisplay(0, 1000));
    
    // Before interval
    TEST_ASSERT_FALSE(shouldRefreshDisplay(lastRefresh, 1000 + 14999));
    
    // At interval
    TEST_ASSERT_TRUE(shouldRefreshDisplay(lastRefresh, 1000 + 15000));
    
    // After interval
    TEST_ASSERT_TRUE(shouldRefreshDisplay(lastRefresh, 1000 + 20000));
}

void test_display_refresh_with_wrap() {
    uint32_t lastRefresh = UINT32_MAX - 5000;
    uint32_t nowWrapped = 10000;
    
    uint32_t elapsed = nowWrapped - lastRefresh;
    TEST_ASSERT_TRUE(elapsed >= kDisplayRefreshIntervalMs);
    TEST_ASSERT_TRUE(shouldRefreshDisplay(lastRefresh, nowWrapped));
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_first_attempt_returns_true);
    RUN_TEST(test_backoff_intervals);
    RUN_TEST(test_backoff_increments_and_caps);
    RUN_TEST(test_backoff_resets);
    RUN_TEST(test_should_attempt_after_interval);
    RUN_TEST(test_data_age_wrap_safe);
    RUN_TEST(test_data_stale_threshold);
    RUN_TEST(test_data_stale_with_wrap);
    RUN_TEST(test_should_refresh_display);
    RUN_TEST(test_display_refresh_with_wrap);
    return UNITY_END();
}
