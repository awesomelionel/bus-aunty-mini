// firmware/src/core/button_gesture.cpp
#include "core/button_gesture.h"

namespace {

// Unsigned subtraction, so an interval that straddles the millis() rollover
// comes out as the small number it really is rather than as nearly 2^32.
bool elapsedAtLeast(uint32_t nowMs, uint32_t sinceMs, uint32_t intervalMs) {
    return static_cast<uint32_t>(nowMs - sinceMs) >= intervalMs;
}

}  // namespace

ButtonGesture::ButtonGesture(uint32_t holdThresholdMs, uint32_t debounceMs)
    : holdThresholdMs_(holdThresholdMs), debounceMs_(debounceMs) {}

void ButtonGesture::setHoldThreshold(uint32_t holdThresholdMs) {
    holdThresholdMs_ = holdThresholdMs;
}

void ButtonGesture::update(bool physicallyPressed, uint32_t nowMs) {
    wasPressed_ = false;
    wasClicked_ = false;
    wasHold_ = false;

    if (physicallyPressed != rawState_) {
        rawState_ = physicallyPressed;
        rawChangedAtMs_ = nowMs;
    }

    // A level is only believed once it has held still for the debounce
    // window, which is what makes a contact bounce a non-event.
    if (rawState_ != pressed_ &&
        elapsedAtLeast(nowMs, rawChangedAtMs_, debounceMs_)) {
        pressed_ = rawState_;
        if (pressed_) {
            pressedAtMs_ = nowMs;
            holdFired_ = false;
            wasPressed_ = true;
        } else if (!holdFired_) {
            wasClicked_ = true;
        }
    }

    if (pressed_ && !holdFired_ &&
        elapsedAtLeast(nowMs, pressedAtMs_, holdThresholdMs_)) {
        holdFired_ = true;
        wasHold_ = true;
    }
}
