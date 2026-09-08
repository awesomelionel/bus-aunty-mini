#include <unity.h>

#include "core/sleep_policy.h"

namespace {

SleepSettings settings() {
    SleepSettings s;
    s.enabled = true;
    s.dimAfterMs = 30000;
    s.sleepAfterMs = 120000;
    return s;
}

IdleInputs idleFor(uint32_t idleMs, uint32_t lastInteractionMs = 0) {
    IdleInputs in;
    in.lastInteractionMs = lastInteractionMs;
    in.nowMs = lastInteractionMs + idleMs;
    in.externallyPowered = false;
    return in;
}

}  // namespace

void test_recent_interaction_stays_awake() {
    TEST_ASSERT_TRUE(PowerMode::Awake == nextPowerMode(settings(), idleFor(0)));
    TEST_ASSERT_TRUE(PowerMode::Awake ==
                     nextPowerMode(settings(), idleFor(29999)));
}

void test_dims_at_the_dim_threshold() {
    TEST_ASSERT_TRUE(PowerMode::Dimmed ==
                     nextPowerMode(settings(), idleFor(30000)));
    TEST_ASSERT_TRUE(PowerMode::Dimmed ==
                     nextPowerMode(settings(), idleFor(119999)));
}

void test_sleeps_at_the_sleep_threshold() {
    TEST_ASSERT_TRUE(PowerMode::Asleep ==
                     nextPowerMode(settings(), idleFor(120000)));
    TEST_ASSERT_TRUE(PowerMode::Asleep ==
                     nextPowerMode(settings(), idleFor(600000)));
}

void test_external_power_never_dims_or_sleeps() {
    IdleInputs in = idleFor(600000);
    in.externallyPowered = true;
    TEST_ASSERT_TRUE(PowerMode::Awake == nextPowerMode(settings(), in));
}

void test_disabled_never_dims_or_sleeps() {
    SleepSettings s = settings();
    s.enabled = false;
    TEST_ASSERT_TRUE(PowerMode::Awake == nextPowerMode(s, idleFor(600000)));
}

void test_survives_the_millis_rollover() {
    // Idle started 40s before the counter wrapped, so the elapsed time has to
    // come out as 40s rather than as very nearly 2^32 ms.
    uint32_t lastInteraction = 0xFFFFFFFFu - 40000u;
    IdleInputs in;
    in.lastInteractionMs = lastInteraction;
    in.nowMs = lastInteraction + 40000u;  // wraps to just past zero
    TEST_ASSERT_TRUE(PowerMode::Dimmed == nextPowerMode(settings(), in));
}

void test_a_zero_dim_delay_dims_immediately() {
    SleepSettings s = settings();
    s.dimAfterMs = 0;
    TEST_ASSERT_TRUE(PowerMode::Dimmed == nextPowerMode(s, idleFor(0)));
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_recent_interaction_stays_awake);
    RUN_TEST(test_dims_at_the_dim_threshold);
    RUN_TEST(test_sleeps_at_the_sleep_threshold);
    RUN_TEST(test_external_power_never_dims_or_sleeps);
    RUN_TEST(test_disabled_never_dims_or_sleeps);
    RUN_TEST(test_survives_the_millis_rollover);
    RUN_TEST(test_a_zero_dim_delay_dims_immediately);
    return UNITY_END();
}
