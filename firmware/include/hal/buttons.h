// firmware/include/hal/buttons.h
#pragma once
#include <cstdint>

namespace hal {

// Buttons by what they do, not by where they are: the shared loop never
// learns a pin number, a label, or a polarity.
//
// A board need not have a distinct button for every role, and may map two
// roles onto one physical button when their gestures do not collide -- the
// Feather's middle button is both Secondary (held) and Sleep (clicked).
// Roles a board has no button for simply never report an event, so callers
// can ask unconditionally; hasButton() is for when the *behaviour* should
// differ, not to guard the queries.
enum class Button {
    Primary,    // advance: next page of services, then on to the next stop
    Previous,   // go back the same way. Absent on two-button boards
    Secondary,  // held, it unlocks the config page
    Sleep,      // clicked, it sleeps the device now
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

// The hold gestures have different thresholds per role, and the loop owns
// those timings rather than the board.
void setHoldThreshold(Button button, uint32_t holdThresholdMs);

}  // namespace hal
