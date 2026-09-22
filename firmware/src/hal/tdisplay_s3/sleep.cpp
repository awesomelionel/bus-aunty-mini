// firmware/src/hal/tdisplay_s3/sleep.cpp
#include "hal/sleep.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "button_pins.h"
#include "hal/buttons.h"

namespace hal {
namespace {

// Sleep is asked for by holding a button, so the button that asked for it is
// usually still down. Left down, it would satisfy the level-triggered wake the
// instant that wake is armed. Generous, because it is the user's finger.
constexpr uint32_t kPreSleepReleaseWaitMs = 5000;
// On the way out the press has already happened; this only covers the tail of
// it. Bounded either way so a stuck button cannot hang the device.
constexpr uint32_t kWakeReleaseWaitMs = 1000;

// Raw pins, not the debounced gestures. After light sleep the gestures still
// think every button is up, so isPressed() would return immediately here and
// the release would then be delivered to the main loop as a click.
bool anyButtonPhysicallyDown() {
    for (const tdisplay::ButtonPin& button : tdisplay::kButtonPins) {
        const bool high = digitalRead(button.gpio) == HIGH;
        if (button.activeLow ? !high : high) {
            return true;
        }
    }
    return false;
}

void waitForButtonRelease(uint32_t timeoutMs) {
    uint32_t startedAt = millis();
    do {
        buttonsUpdate();
        delay(10);
    } while (anyButtonPhysicallyDown() && millis() - startedAt < timeoutMs);

    // The update that accepts the release is also the one that reports the
    // click, and it happens in here rather than in the main loop, so the wake
    // press is consumed instead of being read as a page or a stop change.
    buttonsUpdate();
}

}  // namespace

void sleepUntilButtonPress() {
    waitForButtonRelease(kPreSleepReleaseWaitMs);

    // Both buttons wake the device. Both are active-low, and the pull-up is
    // re-asserted so the level holds through sleep.
    for (const tdisplay::ButtonPin& button : tdisplay::kButtonPins) {
        const gpio_num_t pin = static_cast<gpio_num_t>(button.gpio);
        if (button.activeLow) {
            gpio_pullup_en(pin);
            gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
        } else {
            gpio_pulldown_en(pin);
            gpio_wakeup_enable(pin, GPIO_INTR_HIGH_LEVEL);
        }
    }
    esp_sleep_enable_gpio_wakeup();

    // The peripheral rail on GPIO 15 is deliberately left up through sleep.
    // The vendor's own sleep example drops it, but that is for deep sleep
    // where the panel is re-initialised on the way out anyway; here the panel
    // keeps its own state and only its backlight is off.
    esp_light_sleep_start();

    // Disarm rather than leave a level-triggered wake standing, so the next
    // call arms it from a known state.
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    for (const tdisplay::ButtonPin& button : tdisplay::kButtonPins) {
        gpio_wakeup_disable(static_cast<gpio_num_t>(button.gpio));
    }

    waitForButtonRelease(kWakeReleaseWaitMs);
    buttonsSuppressHeldClicks();
}

}  // namespace hal
