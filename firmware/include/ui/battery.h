// firmware/include/ui/battery.h
#pragma once

// The StickS3 has no fuel gauge; its charge level is derived from battery
// voltage, which sags while WiFi transmits. Readings are smoothed so a single
// transmit does not show up as a drop.
struct BatteryReading {
    int percent = -1;  // -1 when no battery is attached, or nothing read yet
    bool charging = false;
};

// Samples the PMIC at a fixed interval; cheap enough to call every loop.
void batteryPoll();

BatteryReading batteryReading();
