// firmware/src/hal/feather_s3_revtft/button_pins.h
#pragma once
#include <cstdint>

// Board-private, and purely physical: which pin, and which way round. Shared
// by buttons.cpp and sleep.cpp so the polarity is stated once -- the two have
// to agree, and a mismatch would show up as a device that wakes itself the
// instant it sleeps. Which button does what is buttons.cpp's business.
namespace hal::feather {

// The three buttons in the order they sit on the board, top to bottom at
// this rotation.
enum FeatherButton {
    kUp = 0,
    kCenter = 1,
    kDown = 2,
    kFeatherButtonCount = 3,
};

struct ButtonPin {
    uint8_t gpio;
    bool activeLow;
};

// D1 and D2 are plain GPIOs with no external pull, so they are pulled down
// internally and read high when pressed. D0 doubles as the BOOT pin and has
// an external pull-up, so it reads low when pressed.
constexpr ButtonPin kButtonPins[kFeatherButtonCount] = {
    {/*gpio=*/2, /*activeLow=*/false},  // kUp     - D2
    {/*gpio=*/1, /*activeLow=*/false},  // kCenter - D1
    {/*gpio=*/0, /*activeLow=*/true},   // kDown   - D0
};

}  // namespace hal::feather
