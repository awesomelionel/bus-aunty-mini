#include "core/wifi_policy.h"

std::vector<size_t> selectWifiCandidates(
    const std::vector<WifiNetwork>& saved,
    const std::vector<std::string>& seen) {
    std::vector<size_t> indices;
    for (size_t i = 0; i < saved.size(); ++i) {
        for (const std::string& ssid : seen) {
            if (ssid == saved[i].ssid) {
                indices.push_back(i);
                break;
            }
        }
    }
    return indices;
}

uint32_t wifiBackoffMs(uint32_t failedCycles) {
    size_t last =
        sizeof(kWifiBackoffStepsMs) / sizeof(kWifiBackoffStepsMs[0]) - 1;
    size_t i = failedCycles > last ? last : static_cast<size_t>(failedCycles);
    return kWifiBackoffStepsMs[i];
}

bool shouldUpgradeWifi(const std::vector<WifiNetwork>& saved,
                       const std::string& currentSsid,
                       const std::vector<std::string>& seen) {
    if (saved.empty() || currentSsid == saved[0].ssid) {
        return false;
    }
    size_t currentIndex = saved.size();
    for (size_t i = 0; i < saved.size(); ++i) {
        if (saved[i].ssid == currentSsid) {
            currentIndex = i;
            break;
        }
    }
    for (size_t i = 0; i < currentIndex; ++i) {
        for (const std::string& ssid : seen) {
            if (ssid == saved[i].ssid) {
                return true;
            }
        }
    }
    return false;
}
