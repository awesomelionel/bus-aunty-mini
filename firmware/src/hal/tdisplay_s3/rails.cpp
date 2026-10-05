// firmware/src/hal/tdisplay_s3/rails.cpp
#include "rails.h"

#include <Arduino.h>

namespace hal::tdisplay {

void setPeripheralPower(bool on) {
    // LCD_POWER_ON comes from the board variant's pins_arduino.h (GPIO 15).
    pinMode(LCD_POWER_ON, OUTPUT);
    digitalWrite(LCD_POWER_ON, on ? HIGH : LOW);
    if (on) {
        delay(10);
    }
}

void ensurePeripheralPowerOn() {
    static bool powered = false;
    if (powered) {
        return;
    }
    powered = true;
    setPeripheralPower(true);
}

}  // namespace hal::tdisplay
