// firmware/include/core/wifi_credentials.h
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

constexpr size_t kMaxWifiNetworks = 5;
constexpr size_t kWifiSsidMinBytes = 1;
constexpr size_t kWifiSsidMaxBytes = 32;
constexpr size_t kWifiPasswordMinChars = 8;
constexpr size_t kWifiPasswordMaxChars = 63;

struct WifiNetwork {
    std::string ssid;
    std::string password;
};

// SSIDs are stored byte-exact: an iPhone hotspot uses a curly apostrophe, and
// any stripping or case-folding would make that network permanently unjoinable.
bool validateWifiSsid(const std::string& ssid);

// Empty is an open network. Otherwise 8-63 characters. Separators that would
// break the persisted blob are rejected rather than stripped, because a
// silently mutated password fails to authenticate with no visible cause.
bool validateWifiPassword(const std::string& password);

// Drops invalid rows and keeps at most kMaxWifiNetworks. Does not sanitize.
std::vector<WifiNetwork> buildWifiNetworkList(
    const std::vector<WifiNetwork>& rows);

// A blank incoming password copies the password from the first existing row
// with the same SSID. A new SSID with a blank password stays an open network.
std::vector<WifiNetwork> mergeWifiPasswords(
    const std::vector<WifiNetwork>& existing,
    const std::vector<WifiNetwork>& incoming);

std::string serializeWifiNetworks(const std::vector<WifiNetwork>& networks);
std::vector<WifiNetwork> deserializeWifiNetworks(const std::string& blob);

// Last 4 octets of the STA MAC as 8 uppercase hex digits — always a valid
// WPA2 passphrase, and unique per device without being stored.
std::string deriveApPassword(const uint8_t mac[6]);
