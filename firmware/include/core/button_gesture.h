// firmware/include/core/button_gesture.h
#pragma once
#include <cstdint>

// Turns a raw, possibly bouncing button level into click and hold events.
//
// This lives in core/ rather than in a board's HAL because only one of the
// supported boards ships a button library, and having both boards share one
// tested gesture path is worth more than using the library's. The semantics
// are M5Unified's, which the main loop was written against:
//
//   - wasHold() fires exactly once, when the hold threshold is crossed while
//     the button is still down.
//   - wasClicked() fires on release, and only if the hold never fired.
//
// The event queries report what happened during the most recent update() and
// are not cleared by reading, so the same event can be tested more than once
// in a single pass of the loop.
class ButtonGesture {
 public:
    static constexpr uint32_t kDefaultHoldThresholdMs = 500;
    // Long enough to swallow contact bounce, short enough to be invisible.
    static constexpr uint32_t kDefaultDebounceMs = 20;

    explicit ButtonGesture(uint32_t holdThresholdMs = kDefaultHoldThresholdMs,
                           uint32_t debounceMs = kDefaultDebounceMs);

    void setHoldThreshold(uint32_t holdThresholdMs);

    // `physicallyPressed` is the debounced-by-us pin level, already corrected
    // for the board's polarity. `nowMs` is millis(), and may wrap.
    void update(bool physicallyPressed, uint32_t nowMs);

    bool isPressed() const { return pressed_; }
    bool wasPressed() const { return wasPressed_; }
    bool wasClicked() const { return wasClicked_; }
    bool wasHold() const { return wasHold_; }

    // The press that woke the device is not a click: if the finger is still
    // down, the coming release must not page or sleep.
    void suppressClickOnThisPress();

 private:
    uint32_t holdThresholdMs_;
    uint32_t debounceMs_;

    bool rawState_ = false;
    uint32_t rawChangedAtMs_ = 0;

    bool pressed_ = false;
    uint32_t pressedAtMs_ = 0;
    // Latched so one long hold cannot fire twice, and so the release that
    // ends it is not also reported as a click.
    bool holdFired_ = false;

    bool wasPressed_ = false;
    bool wasClicked_ = false;
    bool wasHold_ = false;
};
