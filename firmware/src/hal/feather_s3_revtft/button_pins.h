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
//
// D0 carries the portal gesture despite being BOOT: it is the button a user
// can be told to find ("the bottom one") without reading the silkscreen on
// the back of the board. Holding it for three seconds while the firmware is
// running is harmless -- the ROM bootloader is only entered by holding it
// *across a reset* -- so the worst case is a device that happens to reset
// mid-hold coming up in bootloader mode, which a power cycle undoes.
constexpr ButtonPin kButtonPins[] = {
    {Button::Primary, /*gpio=*/1, /*activeLow=*/false},
    {Button::Secondary, /*gpio=*/0, /*activeLow=*/true},
    {Button::Sleep, /*gpio=*/2, /*activeLow=*/false},
};

}  // namespace hal::feather
