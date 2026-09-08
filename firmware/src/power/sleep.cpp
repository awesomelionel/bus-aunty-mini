// firmware/src/power/sleep.cpp
#include "power/sleep.h"

#include <M5Unified.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

namespace {

// StickS3 buttons: KEY1 (M5.BtnA) and KEY2 (M5.BtnB). Both idle high through a
// pull-up and are pulled to ground when pressed, so a low level is the wake
// condition, and the pull-up is re-asserted below so the level holds through
// sleep. Spelled out as raw GPIO numbers because esp_sleep wants those and
// M5Unified's Button_Class does not expose them.
constexpr gpio_num_t kButtonPins[] = {GPIO_NUM_11, GPIO_NUM_12};

// Sleep is asked for by holding a button, so the button that asked for it is
// usually still down. Left down, it would satisfy the level-triggered wake the
// instant that wake is armed. Generous, because it is the user's finger.
constexpr uint32_t kPreSleepReleaseWaitMs = 5000;
// On the way out the press has already happened; this only covers the tail of
// it. Bounded either way so a stuck button cannot hang the device.
constexpr uint32_t kWakeReleaseWaitMs = 1000;

void waitForButtonRelease(uint32_t timeoutMs) {
    uint32_t startedAt = millis();
    do {
        M5.update();
        delay(10);
    } while ((M5.BtnA.isPressed() || M5.BtnB.isPressed()) &&
             millis() - startedAt < timeoutMs);

    // One more pass after the release settles the click/hold state, so the
    // loop does not also read the press as a page or a stop change.
    M5.update();
}

}  // namespace

void powerSleepUntilButtonPress() {
    waitForButtonRelease(kPreSleepReleaseWaitMs);

    for (gpio_num_t pin : kButtonPins) {
        gpio_pullup_en(pin);
        gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
    }
    esp_sleep_enable_gpio_wakeup();

    esp_light_sleep_start();

    // Disarm rather than leave a level-triggered wake standing, so the next
    // call arms it from a known state.
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    for (gpio_num_t pin : kButtonPins) {
        gpio_wakeup_disable(pin);
    }

    waitForButtonRelease(kWakeReleaseWaitMs);
}
