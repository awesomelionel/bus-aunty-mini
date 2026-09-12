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

// Raw pin, not the debounced gesture. After light sleep the gesture still
// thinks every button is up; using isPressed() here returns immediately and
// the D1 release is then delivered to the main loop as a Sleep click.
bool anyButtonPhysicallyDown() {
    for (const feather::ButtonPin& button : feather::kButtonPins) {
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

    // Drain the debounce window so pressed_ matches the pin, and so the
    // update that reports the click happens here rather than in loop().
    uint32_t drainAt = millis();
    do {
        buttonsUpdate();
        delay(10);
    } while ((isPressed(Button::Primary) || isPressed(Button::Previous) ||
              isPressed(Button::Secondary)) &&
             millis() - drainAt < 30);

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
    buttonsSuppressHeldClicks();
}

}  // namespace hal
