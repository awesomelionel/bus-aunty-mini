// firmware/src/storage/device_settings.cpp
#include "storage/device_settings.h"

#include <Preferences.h>

namespace {

constexpr char kNamespace[] = "busaunty";
constexpr char kAlwaysOnKey[] = "alwayson";

}  // namespace

bool loadAlwaysOn() {
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
        return false;
    }
    bool alwaysOn = prefs.getBool(kAlwaysOnKey, false);
    prefs.end();
    return alwaysOn;
}

bool saveAlwaysOn(bool alwaysOn) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
        return false;
    }
    bool ok = prefs.putBool(kAlwaysOnKey, alwaysOn);
    prefs.end();
    return ok;
}
