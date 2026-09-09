// firmware/src/hal/feather_s3_revtft/rails.cpp
#include "rails.h"

#include <Arduino.h>

namespace hal::feather {

void ensurePeripheralPowerOn() {
    static bool powered = false;
    if (powered) {
        return;
    }
    powered = true;

    pinMode(TFT_I2C_POWER, OUTPUT);
    digitalWrite(TFT_I2C_POWER, HIGH);

    // The NeoPixel is unused and its rail costs current, so it stays down.
    pinMode(NEOPIXEL_POWER, OUTPUT);
    digitalWrite(NEOPIXEL_POWER, LOW);

    delay(10);
}

}  // namespace hal::feather
