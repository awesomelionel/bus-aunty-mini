// firmware/src/hal/feather_s3_revtft/rails.h
#pragma once

// Board-private: the TFT and the I2C bus share one switched rail, so the
// panel and the fuel gauge both have to raise it and neither can assume the
// other went first.
namespace hal::feather {

// Drives TFT_I2C_POWER (GPIO 7) high and lets the rail settle. Nothing on the
// panel or the I2C bus works until this has run, and the failure mode is a
// blank screen with no error. Idempotent.
void ensurePeripheralPowerOn();

}  // namespace hal::feather
