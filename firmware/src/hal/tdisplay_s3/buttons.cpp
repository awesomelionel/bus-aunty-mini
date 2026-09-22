// firmware/src/hal/tdisplay_s3/buttons.cpp
#include "hal/buttons.h"

#include <Arduino.h>

#include "button_pins.h"
#include "core/button_gesture.h"

namespace hal {
namespace {

ButtonGesture gestures[tdisplay::kTDisplayButtonCount];

// Two buttons carrying three roles. The main one advances and, held, sleeps.
// BOOT does double duty the way the Feather's middle button does: clicked it
// steps back, held for three seconds it opens the portal. The two cannot
// collide, because a gesture reports a click only when the hold did not fire
// -- which is what lets a two-button board have back-navigation at all, where
// the StickS3 has to go round the loop the long way.
//
// Sleep has no button of its own, so main.cpp falls back to holding the main
// button instead.
ButtonGesture* gestureFor(Button button) {
    switch (button) {
        case Button::Primary:
            return &gestures[tdisplay::kMain];
        case Button::Previous:
        case Button::Secondary:
            return &gestures[tdisplay::kBoot];
        case Button::Sleep:
            break;
    }
    return nullptr;
}

}  // namespace

void buttonsBegin() {
    for (const tdisplay::ButtonPin& pin : tdisplay::kButtonPins) {
        pinMode(pin.gpio, pin.activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
    }
}

void buttonsUpdate() {
    const uint32_t now = millis();
    for (int i = 0; i < tdisplay::kTDisplayButtonCount; ++i) {
        const tdisplay::ButtonPin& pin = tdisplay::kButtonPins[i];
        const bool high = digitalRead(pin.gpio) == HIGH;
        // Polarity is normalised here so the shared gesture logic never
        // learns how this board's buttons are wired.
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
