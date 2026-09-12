#include <unity.h>

#include "core/button_gesture.h"

namespace {

constexpr uint32_t kHoldMs = 1500;
constexpr uint32_t kDebounceMs = ButtonGesture::kDefaultDebounceMs;

// The loop polls far faster than the debounce window, so the gesture is fed
// at a fixed tick rather than only at the interesting instants.
constexpr uint32_t kTickMs = 5;

// Feeds `pressed` for `durationMs`, starting at `startMs`, and returns the
// time just after the last tick.
uint32_t hold(ButtonGesture& gesture, bool pressed, uint32_t startMs,
              uint32_t durationMs) {
    uint32_t now = startMs;
    for (; now < startMs + durationMs; now += kTickMs) {
        gesture.update(pressed, now);
    }
    return now;
}

}  // namespace

void test_a_short_blip_is_debounced_away() {
    ButtonGesture gesture(kHoldMs);

    uint32_t now = hold(gesture, false, 0, 100);
    // Down for less than the debounce window, so it never counts as a press.
    now = hold(gesture, true, now, 5);
    TEST_ASSERT_FALSE(gesture.isPressed());
    TEST_ASSERT_FALSE(gesture.wasPressed());

    now = hold(gesture, false, now, 200);
    TEST_ASSERT_FALSE(gesture.isPressed());
    TEST_ASSERT_FALSE(gesture.wasClicked());
    TEST_ASSERT_FALSE(gesture.wasHold());
}

void test_a_press_is_reported_once_it_settles() {
    ButtonGesture gesture(kHoldMs);

    uint32_t now = hold(gesture, false, 0, 100);
    now = hold(gesture, true, now, kDebounceMs + kTickMs);
    TEST_ASSERT_TRUE(gesture.isPressed());
}

void test_a_short_press_is_a_click_and_not_a_hold() {
    ButtonGesture gesture(kHoldMs);

    uint32_t now = hold(gesture, false, 0, 100);
    now = hold(gesture, true, now, 300);
    TEST_ASSERT_FALSE(gesture.wasHold());

    // The click lands on the update that accepts the release.
    bool sawClick = false;
    bool sawHold = false;
    for (uint32_t end = now + 200; now < end; now += kTickMs) {
        gesture.update(false, now);
        sawClick |= gesture.wasClicked();
        sawHold |= gesture.wasHold();
    }
    TEST_ASSERT_TRUE(sawClick);
    TEST_ASSERT_FALSE(sawHold);
}

void test_a_long_press_holds_once_and_never_clicks() {
    ButtonGesture gesture(kHoldMs);

    uint32_t now = hold(gesture, false, 0, 100);

    int holds = 0;
    for (uint32_t end = now + kHoldMs * 2; now < end; now += kTickMs) {
        gesture.update(true, now);
        if (gesture.wasHold()) {
            ++holds;
        }
    }
    TEST_ASSERT_EQUAL_INT(1, holds);

    // Releasing after a hold must not also page the screen.
    bool sawClick = false;
    for (uint32_t end = now + 200; now < end; now += kTickMs) {
        gesture.update(false, now);
        sawClick |= gesture.wasClicked();
    }
    TEST_ASSERT_FALSE(sawClick);
}

void test_the_hold_fires_at_the_threshold_not_before() {
    ButtonGesture gesture(kHoldMs);

    uint32_t now = hold(gesture, false, 0, 100);
    uint32_t pressedFrom = now;

    bool sawHold = false;
    // Up to just short of the threshold, nothing should fire. The debounce
    // delays the accepted press, so the real hold comes marginally later.
    for (; now < pressedFrom + kHoldMs; now += kTickMs) {
        gesture.update(true, now);
        sawHold |= gesture.wasHold();
    }
    TEST_ASSERT_FALSE(sawHold);

    for (uint32_t end = now + kDebounceMs + kTickMs * 2; now < end;
         now += kTickMs) {
        gesture.update(true, now);
        sawHold |= gesture.wasHold();
    }
    TEST_ASSERT_TRUE(sawHold);
}

void test_a_second_press_can_hold_again() {
    ButtonGesture gesture(kHoldMs);

    uint32_t now = hold(gesture, false, 0, 100);
    now = hold(gesture, true, now, kHoldMs * 2);
    now = hold(gesture, false, now, 200);

    int holds = 0;
    for (uint32_t end = now + kHoldMs * 2; now < end; now += kTickMs) {
        gesture.update(true, now);
        if (gesture.wasHold()) {
            ++holds;
        }
    }
    TEST_ASSERT_EQUAL_INT(1, holds);
}

// A press that starts just before the counter wraps must not read as having
// been held for very nearly 2^32 ms.
void test_survives_the_millis_rollover() {
    ButtonGesture gesture(kHoldMs);

    uint32_t now = 0xFFFFFFFFu - 40000u;
    now = hold(gesture, false, now, 100);

    bool sawHold = false;
    // Straddle the wrap with a press far shorter than the hold threshold.
    for (uint32_t ticks = 0; ticks < 40; ++ticks, now += kTickMs) {
        gesture.update(true, now);
        sawHold |= gesture.wasHold();
    }
    TEST_ASSERT_TRUE(gesture.isPressed());
    TEST_ASSERT_FALSE(sawHold);

    // And the click on the other side of the wrap still arrives.
    bool sawClick = false;
    for (uint32_t ticks = 0; ticks < 40; ++ticks, now += kTickMs) {
        gesture.update(false, now);
        sawClick |= gesture.wasClicked();
    }
    TEST_ASSERT_TRUE(sawClick);
}

void test_the_hold_threshold_is_configurable() {
    ButtonGesture gesture(kHoldMs);
    gesture.setHoldThreshold(3000);

    uint32_t now = hold(gesture, false, 0, 100);

    bool sawHold = false;
    for (uint32_t end = now + 2000; now < end; now += kTickMs) {
        gesture.update(true, now);
        sawHold |= gesture.wasHold();
    }
    TEST_ASSERT_FALSE(sawHold);

    for (uint32_t end = now + 1200; now < end; now += kTickMs) {
        gesture.update(true, now);
        sawHold |= gesture.wasHold();
    }
    TEST_ASSERT_TRUE(sawHold);
}

void test_suppress_click_on_this_press() {
    ButtonGesture gesture(kHoldMs);

    uint32_t now = hold(gesture, false, 0, 100);
    now = hold(gesture, true, now, 100);
    TEST_ASSERT_TRUE(gesture.isPressed());
    gesture.suppressClickOnThisPress();

    now = hold(gesture, false, now, 100);
    TEST_ASSERT_FALSE(gesture.wasClicked());
    TEST_ASSERT_FALSE(gesture.wasHold());
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_a_short_blip_is_debounced_away);
    RUN_TEST(test_a_press_is_reported_once_it_settles);
    RUN_TEST(test_a_short_press_is_a_click_and_not_a_hold);
    RUN_TEST(test_a_long_press_holds_once_and_never_clicks);
    RUN_TEST(test_the_hold_fires_at_the_threshold_not_before);
    RUN_TEST(test_a_second_press_can_hold_again);
    RUN_TEST(test_survives_the_millis_rollover);
    RUN_TEST(test_the_hold_threshold_is_configurable);
    RUN_TEST(test_suppress_click_on_this_press);
    return UNITY_END();
}
