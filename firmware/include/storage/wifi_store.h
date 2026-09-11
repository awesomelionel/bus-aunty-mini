// firmware/include/storage/wifi_store.h
#pragma once
#include <vector>

#include "core/wifi_credentials.h"

std::vector<WifiNetwork> loadWifiNetworks();
bool saveWifiNetworks(const std::vector<WifiNetwork>& networks);

// True once this firmware has written the wifi list at least once, including
// an empty list. Guards the one-shot import of leftover WiFiManager credentials
// so deleting every network cannot resurrect them.
bool wifiNetworksKeyExists();
