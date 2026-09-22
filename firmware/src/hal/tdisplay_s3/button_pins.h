// firmware/src/hal/tdisplay_s3/button_pins.h
#pragma once
#include <cstdint>

// Board-private, and purely physical: which pin, and which way round. Shared
// by buttons.cpp and sleep.cpp so the polarity is stated once -- the two have
// to agree, and a mismatch would show up as a device that wakes itself the
// instant it sleeps. Which button does what is buttons.cpp's business.
namespace hal::tdisplay {

enum TDisplayButton {
    kMain = 0,
    kBoot = 1,
    kTDisplayButtonCount = 2,
};

struct ButtonPin {
    uint8_t gpio;
    bool activeLow;
};

// Both buttons idle high through a pull-up and are grounded when pressed, so
// unlike the Feather there is no mixed polarity here.
//
// GPIO 0 is also the BOOT strap. That is harmless while the firmware is
// running: the ROM bootloader is only entered by holding it *across a reset*.
constexpr ButtonPin kButtonPins[kTDisplayButtonCount] = {
    {/*gpio=*/14, /*activeLow=*/true},  // kMain
    {/*gpio=*/0, /*activeLow=*/true},   // kBoot
};

}  // namespace hal::tdisplay
