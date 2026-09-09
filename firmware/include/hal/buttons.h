// firmware/include/hal/buttons.h
#pragma once
#include <cstdint>

namespace hal {

// Buttons by what they do, not by where they are: the shared loop never
// learns a pin number, a label, or a polarity.
enum class Button {
    Primary,    // pages through arrivals; held, it sleeps the device
    Secondary,  // held, it reopens the captive portal
    // A dedicated sleep-now button. Absent on boards with only two, which is
    // why every query below tolerates a button that does not exist.
    Sleep,
};

void buttonsBegin();
// Reads the pins and feeds the gestures. Call once per pass of the loop; the
// event queries below describe the most recent call.
void buttonsUpdate();

bool hasButton(Button button);

bool isPressed(Button button);
bool wasPressed(Button button);
bool wasClicked(Button button);
bool wasHold(Button button);

// The two hold gestures have different thresholds, and the loop owns those
// timings rather than the board.
void setHoldThreshold(Button button, uint32_t holdThresholdMs);

}  // namespace hal
