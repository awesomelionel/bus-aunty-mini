// firmware/include/storage/device_settings.h
#pragma once

// Deployment choices that outlive a reboot but are not bus stops.
//
// "Always on" disables the idle dim and sleep stages entirely. It exists
// because whether a device is permanently on mains is a deployment decision
// rather than something the hardware can sense: the Feather's fuel gauge
// cannot tell an absent or fully charged battery from a charging one, so a
// desk clock on USB would otherwise doze off.
bool loadAlwaysOn();
bool saveAlwaysOn(bool alwaysOn);
