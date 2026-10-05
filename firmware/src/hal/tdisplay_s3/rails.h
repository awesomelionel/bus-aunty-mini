// firmware/src/hal/tdisplay_s3/rails.h
#pragma once

// Board-private: this board gates its peripheral rail behind a GPIO, and the
// panel does not come up until that rail does. On battery the same pin lights
// the green V3V indicator.
namespace hal::tdisplay {

// Drives LCD_POWER_ON (GPIO 15). High powers the panel and, on battery, lights
// the green indicator. Low turns both off. After a rising edge the panel
// needs a few milliseconds before it will answer.
void setPeripheralPower(bool on);

// Drives the rail high once and lets it settle. On USB the panel may appear
// to work without this, but on battery nothing is displayed at all -- with
// no error, because the SoC is perfectly happy. Idempotent; a later
// setPeripheralPower(false) is what drops it again for sleep.
void ensurePeripheralPowerOn();

}  // namespace hal::tdisplay
