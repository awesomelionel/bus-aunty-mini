// firmware/src/hal/feather_s3_revtft/sleep.cpp
#include "hal/sleep.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "button_pins.h"
#include "hal/buttons.h"

namespace hal {
namespace {

// Sleep is asked for by pressing or holding a button, so the button that
// asked for it is usually still down. Left down, it would satisfy the
// level-triggered wake the instant that wake is armed. Generous, because it
// is the user's finger.
constexpr uint32_t kPreSleepReleaseWaitMs = 5000;
// On the way out the press has already happened; this only covers the tail of
// it. Bounded either way so a stuck button cannot hang the device.
constexpr uint32_t kWakeReleaseWaitMs = 1000;

bool anyButtonPressed() {
    return isPressed(Button::Primary) || isPressed(Button::Secondary) ||
           isPressed(Button::Sleep);
}

void waitForButtonRelease(uint32_t timeoutMs) {
    uint32_t startedAt = millis();
    do {
        buttonsUpdate();
        delay(10);
    } while (anyButtonPressed() && millis() - startedAt < timeoutMs);

    // The update that accepts the release is also the one that reports the
    // click, and it happens in here rather than in the main loop, so the wake
    // press is consumed instead of being read as a page or a stop change.
    buttonsUpdate();
}

}  // namespace

void sleepUntilButtonPress() {
    waitForButtonRelease(kPreSleepReleaseWaitMs);

    // All three buttons wake the device, each on its own polarity, with the
    // matching internal pull re-asserted so the level holds through sleep.
    for (const feather::ButtonPin& button : feather::kButtonPins) {
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

    // The switched TFT/I2C rail is deliberately left powered through sleep.
    // Cutting it would save more, but every wake would then have to re-init
    // the fuel gauge and rebuild its charge estimate. Worth revisiting if
    // sleep current turns out to matter more than wake latency.
    esp_light_sleep_start();

    // Disarm rather than leave a level-triggered wake standing, so the next
    // call arms it from a known state.
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    for (const feather::ButtonPin& button : feather::kButtonPins) {
        gpio_wakeup_disable(static_cast<gpio_num_t>(button.gpio));
    }

    waitForButtonRelease(kWakeReleaseWaitMs);
}

}  // namespace hal
