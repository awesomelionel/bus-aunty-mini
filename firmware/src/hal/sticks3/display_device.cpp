// firmware/src/hal/sticks3/display_device.cpp
#include "hal/display_device.h"

#include <M5Unified.h>

#include "board/board.h"
#include "m5_init.h"

namespace hal {

Gfx& gfx() { return M5.Display; }

void displayDeviceBegin() {
    sticks3::ensureM5Begun();
    M5.Display.setRotation(board().rotation);
    // Set explicitly rather than left to M5Unified's default, so that
    // undimming has a known level to return to.
    M5.Display.setBrightness(board().brightnessFull);
}

void displayDeviceSetBrightness(uint8_t level) {
    M5.Display.setBrightness(level);
}

void displayDeviceSleep() {
    // Takes the backlight to zero itself, and remembers the level to restore.
    // Setting the brightness to 0 here instead would make that remembered
    // level 0, and the panel would wake up black.
    M5.Display.sleep();
}

void displayDeviceWake() {
    M5.Display.wakeup();
    // wakeup() restores whatever level was set last, which is the dim one when
    // the device dozed off rather than being sent to sleep by hand.
    M5.Display.setBrightness(board().brightnessFull);
}

}  // namespace hal
