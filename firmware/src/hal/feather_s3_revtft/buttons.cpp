// firmware/src/hal/feather_s3_revtft/buttons.cpp
#include "hal/buttons.h"

#include <Arduino.h>

#include "button_pins.h"
#include "core/button_gesture.h"

namespace hal {
namespace {

ButtonGesture primaryGesture;
ButtonGesture secondaryGesture;
ButtonGesture sleepGesture;

ButtonGesture* gestureFor(Button button) {
    switch (button) {
        case Button::Primary:
            return &primaryGesture;
        case Button::Secondary:
            return &secondaryGesture;
        case Button::Sleep:
            return &sleepGesture;
    }
    return nullptr;
}

}  // namespace

void buttonsBegin() {
    for (const feather::ButtonPin& button : feather::kButtonPins) {
        pinMode(button.gpio, button.activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
    }
}

void buttonsUpdate() {
    const uint32_t now = millis();
    for (const feather::ButtonPin& button : feather::kButtonPins) {
        const bool high = digitalRead(button.gpio) == HIGH;
        // Polarity is normalised here so the shared gesture logic never
        // learns that this board's buttons disagree with each other.
        const bool pressed = button.activeLow ? !high : high;
        if (ButtonGesture* gesture = gestureFor(button.button)) {
            gesture->update(pressed, now);
        }
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

}  // namespace hal
