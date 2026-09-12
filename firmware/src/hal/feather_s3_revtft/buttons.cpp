// firmware/src/hal/feather_s3_revtft/buttons.cpp
#include "hal/buttons.h"

#include <Arduino.h>

#include "button_pins.h"
#include "core/button_gesture.h"

namespace hal {
namespace {

ButtonGesture gestures[feather::kFeatherButtonCount];

// Up and down step through the arrivals, and the middle button carries both
// of the remaining roles: a click sleeps, a long hold reopens the portal.
// The two never collide, because a gesture reports a click only when the
// hold did not fire.
//
// The middle button is the portal's, rather than an outer one, because it is
// the only button that can be described to a user without them reading the
// D0/D1/D2 silkscreen on the back of the board.
ButtonGesture* gestureFor(Button button) {
    switch (button) {
        case Button::Primary:
            return &gestures[feather::kUp];
        case Button::Previous:
            return &gestures[feather::kDown];
        case Button::Secondary:
        case Button::Sleep:
            return &gestures[feather::kCenter];
    }
    return nullptr;
}

}  // namespace

void buttonsBegin() {
    for (const feather::ButtonPin& pin : feather::kButtonPins) {
        pinMode(pin.gpio, pin.activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
    }
}

void buttonsUpdate() {
    const uint32_t now = millis();
    for (int i = 0; i < feather::kFeatherButtonCount; ++i) {
        const feather::ButtonPin& pin = feather::kButtonPins[i];
        const bool high = digitalRead(pin.gpio) == HIGH;
        // Polarity is normalised here so the shared gesture logic never
        // learns that this board's buttons disagree with each other.
        gestures[i].update(pin.activeLow ? !high : high, now);
    }
}

bool hasButton(Button button) { return gestureFor(button) != nullptr; }

bool isPressed(Button button) {
    const ButtonGesture* gesture = gestureFor(button);
    return gesture != nullptr && gesture->isPressed();
}

bool wasPressed(Button button) {
    const ButtonGesture* gesture = gestureFor(button);
    return gesture != nullptr && gesture->wasPressed();
}

bool wasClicked(Button button) {
    const ButtonGesture* gesture = gestureFor(button);
    return gesture != nullptr && gesture->wasClicked();
}

bool wasHold(Button button) {
    const ButtonGesture* gesture = gestureFor(button);
    return gesture != nullptr && gesture->wasHold();
}

void setHoldThreshold(Button button, uint32_t holdThresholdMs) {
    if (ButtonGesture* gesture = gestureFor(button)) {
        gesture->setHoldThreshold(holdThresholdMs);
    }
}

void buttonsSuppressHeldClicks() {
    for (ButtonGesture& gesture : gestures) {
        gesture.suppressClickOnThisPress();
    }
}

}  // namespace hal
