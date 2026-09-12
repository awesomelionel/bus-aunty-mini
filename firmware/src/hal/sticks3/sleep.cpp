// firmware/src/hal/sticks3/sleep.cpp
#include "hal/sleep.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "hal/buttons.h"

namespace hal {
namespace {

// KEY1 (Primary) and KEY2 (Secondary). Both idle high through a pull-up and
// are pulled to ground when pressed, so a low level is the wake condition,
// and the pull-up is re-asserted below so the level holds through sleep.
// Spelled out as raw GPIO numbers because esp_sleep wants those and
// M5Unified's Button_Class does not expose them.
struct WakePin {
    gpio_num_t pin;
    bool activeLow;
};
constexpr WakePin kWakePins[] = {
    {GPIO_NUM_11, /*activeLow=*/true},
    {GPIO_NUM_12, /*activeLow=*/true},
};

// Sleep is asked for by holding a button, so the button that asked for it is
// usually still down. Left down, it would satisfy the level-triggered wake the
// instant that wake is armed. Generous, because it is the user's finger.
constexpr uint32_t kPreSleepReleaseWaitMs = 5000;
// On the way out the press has already happened; this only covers the tail of
// it. Bounded either way so a stuck button cannot hang the device.
constexpr uint32_t kWakeReleaseWaitMs = 1000;

bool anyButtonPhysicallyDown() {
    for (const WakePin& wake : kWakePins) {
        const bool high = digitalRead(wake.pin) == HIGH;
        if (wake.activeLow ? !high : high) {
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

    for (const WakePin& wake : kWakePins) {
        if (wake.activeLow) {
            gpio_pullup_en(wake.pin);
            gpio_wakeup_enable(wake.pin, GPIO_INTR_LOW_LEVEL);
        } else {
            gpio_pulldown_en(wake.pin);
            gpio_wakeup_enable(wake.pin, GPIO_INTR_HIGH_LEVEL);
        }
    }
    esp_sleep_enable_gpio_wakeup();

    esp_light_sleep_start();

    // Disarm rather than leave a level-triggered wake standing, so the next
    // call arms it from a known state.
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    for (const WakePin& wake : kWakePins) {
        gpio_wakeup_disable(wake.pin);
    }

    waitForButtonRelease(kWakeReleaseWaitMs);
    buttonsSuppressHeldClicks();
}

}  // namespace hal
