// firmware/src/hal/feather_s3_revtft/button_pins.h
#pragma once
#include <cstdint>

#include "hal/buttons.h"

// Board-private. Shared by buttons.cpp and sleep.cpp so the polarity is
// stated once: the two files have to agree, and a mismatch would show up as
// a device that wakes itself the instant it sleeps.
namespace hal::feather {

struct ButtonPin {
    Button button;
    uint8_t gpio;
    bool activeLow;
};

// D1 and D2 are plain GPIOs with no external pull, so they are pulled down
// internally and read high when pressed. D0 doubles as the BOOT pin and has
// an external pull-up, so it reads low when pressed.
constexpr ButtonPin kButtonPins[] = {
    {Button::Primary, /*gpio=*/1, /*activeLow=*/false},
    {Button::Secondary, /*gpio=*/2, /*activeLow=*/false},
    {Button::Sleep, /*gpio=*/0, /*activeLow=*/true},
};

}  // namespace hal::feather
