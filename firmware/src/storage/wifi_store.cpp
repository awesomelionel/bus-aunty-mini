#include "storage/wifi_store.h"

#include <Preferences.h>

namespace {

constexpr char kNamespace[] = "busaunty";
constexpr char kWifiKey[] = "wifi";
constexpr char kWifiSetKey[] = "wifiset";

}  // namespace

std::vector<WifiNetwork> loadWifiNetworks() {
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
        return {};
    }
    String blob = prefs.getString(kWifiKey, "");
    prefs.end();
    return deserializeWifiNetworks(std::string(blob.c_str()));
}

bool saveWifiNetworks(const std::vector<WifiNetwork>& networks) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
        return false;
    }
    std::string blob = serializeWifiNetworks(networks);
    bool ok;
    if (blob.empty()) {
        ok = prefs.remove(kWifiKey) || !prefs.isKey(kWifiKey);
    } else {
        ok = prefs.putString(kWifiKey, blob.c_str()) == blob.size();
    }
    ok = prefs.putBool(kWifiSetKey, true) && ok;
    prefs.end();
    return ok;
}

bool wifiNetworksKeyExists() {
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
        return false;
    }
    bool exists = prefs.getBool(kWifiSetKey, false);
    prefs.end();
    return exists;
}
