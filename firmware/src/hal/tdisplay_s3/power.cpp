// firmware/src/hal/tdisplay_s3/power.cpp
#include "hal/power.h"

#include <Arduino.h>

namespace hal {
namespace {

// Same cadence and smoothing as the other two boards, so the icon behaves
// identically everywhere even though what feeds it differs.
constexpr uint32_t kSampleIntervalMs = 2000;
constexpr float kSmoothing = 0.2f;

// The battery sits behind a half divider on this pin, so the cell voltage is
// twice what the ADC sees.
constexpr uint8_t kBatteryAdcPin = 4;
constexpr int kDividerRatio = 2;

// A single li-ion cell, mapped linearly between these. Crude, but the reading
// is smoothed and only ever drawn as a five-step bar.
constexpr int kEmptyMillivolts = 3300;
constexpr int kFullMillivolts = 4200;
// With no battery attached the pin reads the charger rail instead, which sits
// above anything a real cell reaches. That is what lets this board tell an
// absent battery from a full one, unlike the Feather.
constexpr int kNoBatteryMillivolts = 4300;

float smoothedPercent = -1.0f;
bool batteryPresent = false;
uint32_t lastSampleMillis = 0;
bool sampledOnce = false;

int readCellMillivolts() {
    // analogReadMilliVolts applies the chip's own ADC calibration. The
    // vendor's example uses esp_adc_cal directly, which is the ESP-IDF 4 API
    // and is not what this core exposes for the S3.
    return static_cast<int>(analogReadMilliVolts(kBatteryAdcPin)) *
           kDividerRatio;
}

}  // namespace

void powerBegin() {
    // The divider feeds an ADC1 channel, which needs no bus and no rail of
    // its own, so there is nothing to bring up here. Kept for the interface.
    analogReadResolution(12);
}

void powerPoll() {
    uint32_t now = millis();
    if (sampledOnce && now - lastSampleMillis < kSampleIntervalMs) {
        return;
    }
    sampledOnce = true;
    lastSampleMillis = now;

    const int millivolts = readCellMillivolts();

    if (millivolts > kNoBatteryMillivolts) {
        batteryPresent = false;
        smoothedPercent = -1.0f;
        return;
    }
    batteryPresent = true;

    int level = (millivolts - kEmptyMillivolts) * 100 /
                (kFullMillivolts - kEmptyMillivolts);
    if (level < 0) {
        level = 0;
    } else if (level > 100) {
        level = 100;
    }

    smoothedPercent =
        smoothedPercent < 0.0f
            ? static_cast<float>(level)
            : smoothedPercent + kSmoothing * (level - smoothedPercent);
}

PowerStatus powerStatus() {
    PowerStatus status;
    // This board exposes no charger status line, so charging is never
    // reported and the icon never greens. The absent-battery reading below is
    // the only supply signal available.
    status.charging = false;
    if (smoothedPercent >= 0.0f) {
        status.percent = static_cast<int>(smoothedPercent + 0.5f);
    }
    // No battery attached means there is nothing to conserve, so the device
    // is being run from USB and should stay lit.
    status.externalPower = sampledOnce && !batteryPresent;
    return status;
}

}  // namespace hal
