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

// Draws the arrivals screen as a Windows 95 window -- title bar, column
// headers, status bar -- instead of the plain list. Off by default, because
// the chrome costs a service row and only the 320x170 panel has the height to
// spare; boards that do not, say so through BoardProfile and never offer it.
bool loadWin95Theme();
bool saveWin95Theme(bool enabled);
