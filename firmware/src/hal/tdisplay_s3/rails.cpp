// firmware/src/hal/tdisplay_s3/rails.cpp
#include "rails.h"

#include <Arduino.h>

namespace hal::tdisplay {

void ensurePeripheralPowerOn() {
    static bool powered = false;
    if (powered) {
        return;
    }
    powered = true;

    // LCD_POWER_ON comes from the board variant's pins_arduino.h (GPIO 15).
    pinMode(LCD_POWER_ON, OUTPUT);
    digitalWrite(LCD_POWER_ON, HIGH);

    delay(10);
}

}  // namespace hal::tdisplay
