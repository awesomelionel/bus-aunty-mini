// firmware/src/storage/bus_stop_store.cpp
#include "storage/bus_stop_store.h"

#include <Preferences.h>

namespace {

constexpr char kNamespace[] = "busaunty";
constexpr char kStopsKey[] = "stops";

}  // namespace

std::vector<BusStopConfig> loadBusStops() {
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
        return {};
    }
    String blob = prefs.getString(kStopsKey, "");
    prefs.end();
    return deserializeBusStops(std::string(blob.c_str()));
}

bool saveBusStops(const std::vector<BusStopConfig>& stops) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
        return false;
    }
    std::string blob = serializeBusStops(stops);
    bool ok;
    if (blob.empty()) {
        // Clearing every stop drops the key instead of storing an empty value,
        // which keeps loadBusStops on its "nothing configured" path.
        ok = prefs.remove(kStopsKey) || !prefs.isKey(kStopsKey);
    } else {
        ok = prefs.putString(kStopsKey, blob.c_str()) == blob.size();
    }
    prefs.end();
    return ok;
}
