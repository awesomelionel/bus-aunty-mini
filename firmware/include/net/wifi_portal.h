// firmware/include/net/wifi_portal.h
#pragma once
#include <functional>
#include <vector>

#include "core/bus_stop_config.h"

// Called when the captive portal comes up, so the caller can draw the setup
// screen with the AP name.
using PortalStartedCallback = std::function<void()>;

// `stops` and `alwaysOn` are read to seed the form and written back with
// whatever the user saved.
//
// Brings up WiFi, falling back to the captive portal when there are no working
// credentials. Returns false only when the portal timed out without a config.
bool wifiPortalConnect(const char* apSsid, std::vector<BusStopConfig>* stops,
                       bool* alwaysOn,
                       const PortalStartedCallback& onPortalStarted);

// Opens the portal on demand, even when WiFi is already up, so bus stops can
// be edited after the initial setup. Returns true if the user saved.
bool wifiPortalReconfigure(const char* apSsid,
                           std::vector<BusStopConfig>* stops, bool* alwaysOn,
                           const PortalStartedCallback& onPortalStarted);
