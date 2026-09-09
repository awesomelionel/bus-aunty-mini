// firmware/src/hal/sticks3/power.cpp
#include "hal/power.h"

#include <M5Unified.h>

#include "m5_init.h"

namespace hal {
namespace {

// The StickS3 has no fuel gauge; its charge level is derived from battery
// voltage, which sags while WiFi transmits. Readings are smoothed so a single
// transmit does not show up as a drop.
constexpr uint32_t kSampleIntervalMs = 2000;
// Around 15 samples land between two arrival fetches, so the displayed value
// settles well within one poll cycle while any single dip barely moves it.
constexpr float kSmoothing = 0.2f;

float smoothedPercent = -1.0f;
bool charging = false;
uint32_t lastSampleMillis = 0;
bool sampledOnce = false;

}  // namespace

void powerBegin() { sticks3::ensureM5Begun(); }

void powerPoll() {
    uint32_t now = millis();
    if (sampledOnce && now - lastSampleMillis < kSampleIntervalMs) {
        return;
    }
    sampledOnce = true;
    lastSampleMillis = now;

    charging = M5.Power.isCharging() == m5::Power_Class::is_charging;

    int32_t level = M5.Power.getBatteryLevel();
    if (level < 0) {
        // No battery, or the PMIC has not settled yet just after boot.
        smoothedPercent = -1.0f;
        return;
    }

    smoothedPercent =
        smoothedPercent < 0.0f
            ? static_cast<float>(level)
            : smoothedPercent + kSmoothing * (level - smoothedPercent);
}

PowerStatus powerStatus() {
    PowerStatus status;
    status.charging = charging;
    if (smoothedPercent >= 0.0f) {
        status.percent = static_cast<int>(smoothedPercent + 0.5f);
    }
    // A negative percentage means no battery is attached, so the device is on
    // USB and has nothing to conserve. A fully charged device on USB reports
    // neither charging nor a negative level, so it will still dim and sleep;
    // one press brings it back.
    status.externalPower = status.charging || status.percent < 0;
    return status;
}

}  // namespace hal
