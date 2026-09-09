// firmware/include/hal/display_device.h
#pragma once
#include <cstdint>

#include "hal/gfx_types.h"

namespace hal {

// The panel to draw onto. Valid before displayDeviceBegin() only as an
// address to bind a sprite to; drawing needs the panel brought up first.
Gfx& gfx();

// Powers whatever rails the panel needs, initialises it, and applies the
// board's rotation and full brightness.
void displayDeviceBegin();

void displayDeviceSetBrightness(uint8_t level);

// Puts the panel in its own low-power state. The caller is expected to have
// cleared the screen first: the panel keeps its frame buffer.
void displayDeviceSleep();
// Brings the panel back at full brightness, whatever level was set before.
void displayDeviceWake();

}  // namespace hal
