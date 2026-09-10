// firmware/src/hal/sticks3/buttons.cpp
#include "hal/buttons.h"

#include <M5Unified.h>

#include "core/button_gesture.h"
#include "m5_init.h"

namespace hal {
namespace {

ButtonGesture primaryGesture;
ButtonGesture secondaryGesture;

// Fed from M5's raw isPressed() rather than its own wasClicked()/wasHold(),
// so this board and the Feather share one tested gesture path instead of two
// implementations that have to be kept agreeing.
ButtonGesture* gestureFor(Button button) {
    switch (button) {
        case Button::Primary:
            return &primaryGesture;
        case Button::Secondary:
            return &secondaryGesture;
        case Button::Previous:
        case Button::Sleep:
            // Only KEY1 and KEY2 here, so there is nothing to spare for
            // going back or for a dedicated sleep button. Holding KEY1
            // sleeps instead.
            break;
    }
    return nullptr;
}

}  // namespace

void buttonsBegin() { sticks3::ensureM5Begun(); }

void buttonsUpdate() {
    M5.update();
    const uint32_t now = millis();
    primaryGesture.update(M5.BtnA.isPressed(), now);
    secondaryGesture.update(M5.BtnB.isPressed(), now);
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
