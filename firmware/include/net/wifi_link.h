// firmware/include/net/wifi_link.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/wifi_credentials.h"

enum class WifiLinkState {
    Scanning,
    FastPath,
    Connecting,
    Connected,
    Backoff,
    ApFallback,
};

void wifiLinkBegin(const std::vector<WifiNetwork>& networks);
void wifiLinkSetNetworks(const std::vector<WifiNetwork>& networks);
void wifiLinkTick(uint32_t nowMs);

// Cancels a backoff wait and starts a scan. A no-op while connected or
// while the setup AP is up, so paging around arrivals cannot drop WiFi.
void wifiLinkForceScan();

void wifiLinkStartAp(const char* ssid, const char* password);
void wifiLinkStopAp();

// Same teardown enterSleep already used: disconnect, then WIFI_OFF.
void wifiLinkPrepareSleep();
void wifiLinkOnWake();

// Hold upgrade scans while a fetch is in flight so a switch cannot
// interrupt HTTP.
void wifiLinkSetBusy(bool busy);

// Fills the last completed scan. Used by the config page dropdown.
void wifiLinkRequestVisibleScan();
std::vector<std::string> wifiLinkVisibleSsids();

// Reads whatever the ESP-IDF station driver loaded from its own NVS.
bool wifiLinkImportStaCredentials(WifiNetwork* out);

WifiLinkState wifiLinkState();
bool wifiLinkConnected();
std::string wifiLinkCurrentSsid();
std::string wifiLinkIp();
uint32_t wifiLinkBackoffEndsAt();
