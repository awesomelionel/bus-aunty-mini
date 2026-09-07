// firmware/src/ui/battery.cpp
#include "ui/battery.h"

#include <M5Unified.h>

namespace {

constexpr uint32_t kSampleIntervalMs = 2000;
// Around 15 samples land between two arrival fetches, so the displayed value
// settles well within one poll cycle while any single dip barely moves it.
constexpr float kSmoothing = 0.2f;

float smoothedPercent = -1.0f;
bool charging = false;
uint32_t lastSampleMillis = 0;
bool sampledOnce = false;

}  // namespace

void batteryPoll() {
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

BatteryReading batteryReading() {
    BatteryReading reading;
    reading.charging = charging;
    if (smoothedPercent >= 0.0f) {
        reading.percent = static_cast<int>(smoothedPercent + 0.5f);
    }
    return reading;
}
