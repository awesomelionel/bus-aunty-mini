// firmware/src/hal/feather_s3_revtft/power.cpp
#include "hal/power.h"

#include <Adafruit_MAX1704X.h>
#include <Arduino.h>
#include <Wire.h>

#include "rails.h"

namespace hal {
namespace {

// Same cadence and smoothing as the StickS3, so the icon behaves identically
// on both boards even though a fuel gauge is steadier than a voltage divider.
constexpr uint32_t kSampleIntervalMs = 2000;
constexpr float kSmoothing = 0.2f;

Adafruit_MAX17048 gauge;
bool gaugeReady = false;
float smoothedPercent = -1.0f;
bool charging = false;
uint32_t lastSampleMillis = 0;
bool sampledOnce = false;

}  // namespace

void powerBegin() {
    // The gauge shares the switched rail with the panel, so this has to be up
    // before the I2C bus is touched.
    feather::ensurePeripheralPowerOn();

    Wire.begin(SDA, SCL);
    gaugeReady = gauge.begin(&Wire);
}

void powerPoll() {
    uint32_t now = millis();
    if (sampledOnce && now - lastSampleMillis < kSampleIntervalMs) {
        return;
    }
    sampledOnce = true;
    lastSampleMillis = now;

    if (!gaugeReady) {
        // Retried rather than given up on: the rail may still have been
        // settling when powerBegin() ran, and one early failure should not
        // cost the battery icon for the rest of the session.
        gaugeReady = gauge.begin(&Wire);
        if (!gaugeReady) {
            smoothedPercent = -1.0f;
            charging = false;
            return;
        }
    }

    charging = gauge.chargeRate() > 0.0f;

    float level = gauge.cellPercent();
    if (level < 0.0f) {
        level = 0.0f;
    } else if (level > 100.0f) {
        level = 100.0f;
    }

    smoothedPercent = smoothedPercent < 0.0f
                          ? level
                          : smoothedPercent +
                                kSmoothing * (level - smoothedPercent);
}

PowerStatus powerStatus() {
    PowerStatus status;
    status.charging = charging;
    if (smoothedPercent >= 0.0f) {
        status.percent = static_cast<int>(smoothedPercent + 0.5f);
    }
    // Charging is the only supply signal this board has. With no battery
    // attached the gauge reads the charger rail and reports a healthy charge,
    // so a high percentage says nothing about whether we are on mains --
    // which is what BoardProfile::detectsBatteryPresence being false means.
    // Deployments that are permanently on USB turn the idle stages off in the
    // portal instead.
    status.externalPower = status.charging;
    return status;
}

}  // namespace hal
