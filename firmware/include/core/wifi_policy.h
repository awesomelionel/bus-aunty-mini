// firmware/include/core/wifi_policy.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/wifi_credentials.h"

constexpr uint32_t kWifiBackoffStepsMs[] = {5000, 15000, 60000};
constexpr uint32_t kWifiUpgradeIntervalMs = 300000;
constexpr uint32_t kWifiConnectTimeoutMs = 10000;
constexpr uint32_t kWifiUnlockWindowMs = 300000;

// Saved-list indices of networks whose SSID appears in `seen`, in saved
// order — that order is the priority, not signal strength.
std::vector<size_t> selectWifiCandidates(
    const std::vector<WifiNetwork>& saved,
    const std::vector<std::string>& seen);

// failedCycles is how many full scan-and-connect cycles have failed in a row.
uint32_t wifiBackoffMs(uint32_t failedCycles);

// True when a higher-priority saved network than `currentSsid` is in `seen`.
// An unknown current SSID is treated as below every saved network.
bool shouldUpgradeWifi(const std::vector<WifiNetwork>& saved,
                       const std::string& currentSsid,
                       const std::vector<std::string>& seen);
