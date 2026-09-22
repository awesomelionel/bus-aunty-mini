// firmware/src/storage/device_settings.cpp
#include "storage/device_settings.h"

#include <Preferences.h>

namespace {

constexpr char kNamespace[] = "busaunty";
constexpr char kAlwaysOnKey[] = "alwayson";
constexpr char kWin95ThemeKey[] = "win95";

bool loadFlag(const char* key, bool fallback) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
        return fallback;
    }
    bool value = prefs.getBool(key, fallback);
    prefs.end();
    return value;
}

bool saveFlag(const char* key, bool value) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
        return false;
    }
    bool ok = prefs.putBool(key, value);
    prefs.end();
    return ok;
}

}  // namespace

bool loadAlwaysOn() { return loadFlag(kAlwaysOnKey, false); }

bool saveAlwaysOn(bool alwaysOn) { return saveFlag(kAlwaysOnKey, alwaysOn); }

bool loadWin95Theme() { return loadFlag(kWin95ThemeKey, false); }

bool saveWin95Theme(bool enabled) { return saveFlag(kWin95ThemeKey, enabled); }
