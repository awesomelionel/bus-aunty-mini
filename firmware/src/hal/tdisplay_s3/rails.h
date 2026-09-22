// firmware/src/hal/tdisplay_s3/rails.h
#pragma once

// Board-private: this board gates its peripheral rail behind a GPIO, and the
// panel does not come up until that rail does.
namespace hal::tdisplay {

// Drives LCD_POWER_ON (GPIO 15) high and lets the rail settle. On USB the
// panel may appear to work without this, but on battery nothing is displayed
// at all -- with no error, because the SoC is perfectly happy. Idempotent.
void ensurePeripheralPowerOn();

}  // namespace hal::tdisplay
