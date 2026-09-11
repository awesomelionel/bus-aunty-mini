#include "core/wifi_credentials.h"

#include <cstdio>

namespace {

constexpr char kFieldSep = '\x1f';
constexpr char kRecordSep = '\x1e';

bool containsSeparator(const std::string& s) {
    return s.find(kFieldSep) != std::string::npos ||
           s.find(kRecordSep) != std::string::npos;
}

}  // namespace

bool validateWifiSsid(const std::string& ssid) {
    return ssid.size() >= kWifiSsidMinBytes &&
           ssid.size() <= kWifiSsidMaxBytes && !containsSeparator(ssid);
}

bool validateWifiPassword(const std::string& password) {
    if (containsSeparator(password)) {
        return false;
    }
    if (password.empty()) {
        return true;
    }
    return password.size() >= kWifiPasswordMinChars &&
           password.size() <= kWifiPasswordMaxChars;
}

std::vector<WifiNetwork> buildWifiNetworkList(
    const std::vector<WifiNetwork>& rows) {
    std::vector<WifiNetwork> networks;
    for (const WifiNetwork& row : rows) {
        if (networks.size() >= kMaxWifiNetworks) {
            break;
        }
        if (!validateWifiSsid(row.ssid) || !validateWifiPassword(row.password)) {
            continue;
        }
        networks.push_back(row);
    }
    return networks;
}

std::vector<WifiNetwork> mergeWifiPasswords(
    const std::vector<WifiNetwork>& existing,
    const std::vector<WifiNetwork>& incoming) {
    std::vector<WifiNetwork> rows = incoming;
    for (WifiNetwork& row : rows) {
        if (!row.password.empty()) {
            continue;
        }
        for (const WifiNetwork& prior : existing) {
            if (prior.ssid == row.ssid) {
                row.password = prior.password;
                break;
            }
        }
    }
    return buildWifiNetworkList(rows);
}

std::string serializeWifiNetworks(const std::vector<WifiNetwork>& networks) {
    std::string blob;
    for (size_t i = 0; i < networks.size(); ++i) {
        if (i > 0) {
            blob.push_back(kRecordSep);
        }
        blob += networks[i].ssid;
        blob.push_back(kFieldSep);
        blob += networks[i].password;
    }
    return blob;
}

std::vector<WifiNetwork> deserializeWifiNetworks(const std::string& blob) {
    std::vector<WifiNetwork> rows;
    if (blob.empty()) {
        return {};
    }
    size_t start = 0;
    while (start <= blob.size()) {
        size_t recordEnd = blob.find(kRecordSep, start);
        if (recordEnd == std::string::npos) {
            recordEnd = blob.size();
        }
        std::string record = blob.substr(start, recordEnd - start);
        size_t fieldEnd = record.find(kFieldSep);
        if (fieldEnd != std::string::npos) {
            WifiNetwork row;
            row.ssid = record.substr(0, fieldEnd);
            row.password = record.substr(fieldEnd + 1);
            rows.push_back(row);
        }
        if (recordEnd == blob.size()) {
            break;
        }
        start = recordEnd + 1;
    }
    return buildWifiNetworkList(rows);
}

std::string deriveApPassword(const uint8_t mac[6]) {
    char buf[9];
    std::snprintf(buf, sizeof(buf), "%02X%02X%02X%02X", mac[2], mac[3], mac[4],
                  mac[5]);
    return buf;
}
