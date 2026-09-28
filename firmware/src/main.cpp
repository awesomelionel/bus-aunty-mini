// firmware/src/main.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <time.h>

#include <array>
#include <string>
#include <vector>

#include "core/arrival_parser.h"
#include "core/arrival_screen.h"
#include "core/backoff_scheduler.h"
#include "core/bus_stop_config.h"
#include "core/night_window.h"
#include "core/sleep_policy.h"
#include "core/wifi_credentials.h"
#include "hal/buttons.h"
#include "hal/power.h"
#include "hal/sleep.h"
#include "net/bus_api_client.h"
#include "net/config_server.h"
#include "net/wifi_link.h"
#include "storage/bus_stop_store.h"
#include "storage/device_settings.h"
#include "storage/wifi_store.h"
#include "ui/display.h"

namespace {

constexpr uint32_t kPortalHoldMs = 3000;
constexpr uint32_t kSleepHoldMs = 1500;
constexpr char kSetupApSsid[] = "BusAuntySetup";

constexpr uint32_t kDimAfterMs = 30000;
constexpr uint32_t kSleepAfterMs = 120000;
constexpr uint32_t kSleepNoticeMs = 700;
constexpr uint32_t kPortalNoticeMs = 900;
// The Feather's middle button is both wake and Sleep. A tap that woke us
// can still be in flight as a click after the HAL release-wait; ignore it
// for a beat so that press cannot put the device straight back under.
constexpr uint32_t kIgnoreSleepClickAfterWakeMs = 1500;

std::vector<BusStopConfig> busStops;
std::vector<WifiNetwork> wifiNetworks;
bool alwaysOn = false;
bool win95Theme = false;
bool networksDirty = false;
bool stopsDirty = false;
bool alwaysOnDirty = false;
bool win95ThemeDirty = false;
size_t currentStopIndex = 0;
BackoffState backoffState;
bool needsImmediateFetch = true;
uint32_t lastDisplayRefreshMs = 0;
bool noStopsRendered = false;
bool offlineRendered = false;
bool connectingRendered = false;
bool inConfigScreen = false;
bool timeSynced = false;
WifiLinkState lastLinkState = WifiLinkState::Backoff;

uint32_t lastInteractionMillis = 0;
uint32_t ignoreSleepClickUntilMs = 0;
PowerMode powerMode = PowerMode::Awake;

// Per-stop cache: stores rows and fetch time for each configured stop
struct StopCache {
    std::string stopCode;
    std::vector<BusServiceRow> rows;
    std::string label;
    uint32_t fetchedAtMillis = 0;
    int64_t updatedAtEpoch = -1;
    bool hadArrivals = false;
    bool notFound = false;
    bool valid = false;
};
std::array<StopCache, kMaxBusStops> stopCaches;

size_t currentPage = 0;

void noteInteraction() { lastInteractionMillis = millis(); }

void logWifiDiagnostics() {
    Serial.printf("[boot] reset reason %d, free heap %u\n",
                  static_cast<int>(esp_reset_reason()), ESP.getFreeHeap());

    WiFi.onEvent(
        [](arduino_event_id_t, arduino_event_info_t info) {
            const auto& e = info.wifi_sta_connected;
            Serial.printf("[wifi] associated: channel %u, authmode %u\n",
                          e.channel, static_cast<unsigned>(e.authmode));
        },
        ARDUINO_EVENT_WIFI_STA_CONNECTED);

    WiFi.onEvent(
        [](arduino_event_id_t, arduino_event_info_t info) {
            const auto& e = info.wifi_sta_disconnected;
            Serial.printf("[wifi] disconnected: reason %u, rssi %d\n", e.reason,
                          e.rssi);
        },
        ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.onEvent(
        [](arduino_event_id_t, arduino_event_info_t) {
            Serial.printf("[wifi] got ip %s\n",
                          WiFi.localIP().toString().c_str());
        },
        ARDUINO_EVENT_WIFI_STA_GOT_IP);
}

void persistDirtySettings() {
    const bool saved = stopsDirty || alwaysOnDirty || win95ThemeDirty ||
                       networksDirty;
    if (stopsDirty) {
        saveBusStops(busStops);
        stopsDirty = false;
        // Clear all caches when stops change
        for (StopCache& cache : stopCaches) {
            cache.valid = false;
            cache.rows.clear();
            cache.stopCode.clear();
            cache.notFound = false;
            cache.hadArrivals = false;
        }
        currentPage = 0;
        needsImmediateFetch = wifiLinkConnected();
    }
    if (alwaysOnDirty) {
        saveAlwaysOn(alwaysOn);
        alwaysOnDirty = false;
    }
    if (win95ThemeDirty) {
        saveWin95Theme(win95Theme);
        win95ThemeDirty = false;
        // Applied here as well as saved, because this is the one place that
        // knows the setting just changed. The chrome costs a row, so the
        // layout and the paging both have to be rebuilt: a page index from
        // the old five-row screen can point past the end of a four-row one.
        displaySetTheme(win95Theme);
        currentPage = 0;
        // Invalidate all stop caches
        for (StopCache& cache : stopCaches) {
            cache.valid = false;
            cache.rows.clear();
            cache.notFound = false;
            cache.hadArrivals = false;
        }
        needsImmediateFetch = wifiLinkConnected();
    }
    if (networksDirty) {
        saveWifiNetworks(wifiNetworks);
        networksDirty = false;
    }
    // A config save starts the retry schedule over, the same as a good fetch.
    if (saved) {
        resetBackoff(backoffState);
    }
}

void startSetupAp() {
    uint8_t mac[6] = {};
    WiFi.mode(WIFI_STA);
    WiFi.macAddress(mac);
    std::string password = deriveApPassword(mac);
    wifiLinkStartAp(kSetupApSsid, password.c_str());
    configServerUnlock(millis());
    configServerStartCaptiveDns();
    displayShowWifiSetup(kSetupApSsid, password);
    inConfigScreen = false;
    offlineRendered = false;
}

void syncTime() {
    displayShowStatus("Syncing time...");
    // The offset reaches localtime() only: configTime() sets the TZ
    // environment variable and never touches the system clock, so time()
    // still returns UTC epoch seconds and every arrival calculation is
    // unaffected. It is here so the framed screen's clock and its night
    // window read as Singapore rather than UTC.
    configTime(kLocalUtcOffsetSeconds, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    uint32_t startedAt = millis();
    while (now < 1700000000 && millis() - startedAt < 15000) {
        delay(250);
        now = time(nullptr);
    }
    timeSynced = now >= 1700000000;
}

bool cachedRowsHaveLabels(const std::vector<BusServiceRow>& rows) {
    for (const BusServiceRow& row : rows) {
        if (rowShowsLabel(row)) {
            return true;
        }
    }
    return false;
}

void showArrivalScreen(ArrivalScreen screen, const StopCache& cache) {
    const std::string hm = formatLocalHm(cache.updatedAtEpoch);
    switch (screen) {
        case ArrivalScreen::Loading:
            displayShowStatus("Loading...");
            break;
        case ArrivalScreen::NotFound:
            displayShowStatus("No data\nCheck stop code");
            break;
        case ArrivalScreen::NoRecentData:
            displayShowStatus("No data\nLast: " + hm);
            break;
        case ArrivalScreen::NoServices:
            displayShowStatus(cache.label + ": no services");
            break;
        case ArrivalScreen::NoMoreBuses:
            displayShowStatus("No More Buses\nUpdated " + hm);
            break;
        case ArrivalScreen::Arrivals:
            break;
    }
}

void renderCachedPage() {
    if (currentStopIndex >= kMaxBusStops || currentStopIndex >= busStops.size()) {
        return;
    }
    
    StopCache& cache = stopCaches[currentStopIndex];
    if (!cache.valid) {
        return;
    }
    
    // Verify cache stopCode matches current stop
    if (cache.stopCode != busStops[currentStopIndex].code) {
        cache.valid = false;
        return;
    }
    
    const int64_t nowEpoch = time(nullptr);
    const bool clockSet = nowEpoch >= kClockSetEpoch;
    std::vector<BusServiceRow> live = cache.rows;
    if (clockSet) {
        pruneExpiredArrivals(live, nowEpoch);
    }
    const uint32_t ageMs = dataAgeMs(cache.fetchedAtMillis, millis());
    const ArrivalScreen screen = selectArrivalScreen(
        clockSet, cache.notFound, cache.hadArrivals, !live.empty(), ageMs);
    if (screen != ArrivalScreen::Arrivals) {
        showArrivalScreen(screen, cache);
        return;
    }

    bool hasLabels = cachedRowsHaveLabels(live);
    size_t totalPages =
        servicePageCount(live, servicesPerScreen(hasLabels));
    if (currentPage >= totalPages) {
        currentPage = 0;
    }
    std::vector<BusServiceRow> page =
        selectServicePage(live, servicesPerScreen(hasLabels), currentPage);

    displayShowArrivals(cache.label, page, nowEpoch, currentStopIndex,
                         busStops.size(), currentPage, totalPages,
                         hal::powerStatus(), ageMs, cache.updatedAtEpoch,
                         !wifiLinkConnected());
}

void pollAndRender() {
    if (currentStopIndex >= busStops.size() || currentStopIndex >= kMaxBusStops) {
        return;
    }
    
    const BusStopConfig& stop = busStops[currentStopIndex];
    const std::string& label = busStopLabel(stop);
    StopCache& cache = stopCaches[currentStopIndex];
    
    // Errors keep the last good payload. The 10-minute screen is applied
    // when that payload is drawn, not by discarding it here.
    bool haveCache = cache.valid;
    
    // Only show "Loading..." on first fetch or stop change
    if (!cache.valid) {
        displayShowStatus("Loading " + label + "...");
    }

    // The retry interval is measured from the start of this attempt.
    backoffState.lastAttemptMs = millis();
    
    wifiLinkSetBusy(true);
    FetchResult fetch = fetchBusArrival(stop.code);
    wifiLinkSetBusy(false);

    // Status first. Error bodies are plain text, so they are not parsed.
    if (classifyFetch(fetch.httpStatus, false) == FetchClass::NotFound) {
        cache.stopCode = stop.code;
        cache.label = label;
        cache.notFound = true;
        cache.valid = true;
        renderCachedPage();
        return;
    }
    if (fetch.httpStatus != 200) {
        incrementBackoff(backoffState);
        if (haveCache) {
            renderCachedPage();
        } else {
            displayShowStatus(std::string("Fetch failed (") +
                              std::to_string(fetch.httpStatus) + ")");
        }
        return;
    }

    ParsedBusStop parsed = parseBusArrivalResponse(fetch.body, stop.code);
    if (classifyFetch(fetch.httpStatus, parsed.valid) != FetchClass::Ok) {
        // Parse error, or a 200 whose busStops list does not contain this stop.
        incrementBackoff(backoffState);
        if (haveCache) {
            renderCachedPage();
        } else if (fetch.parseError) {
            displayShowStatus("Bad data");
        } else {
            displayShowStatus("Bad response for " + label);
        }
        return;
    }

    const int64_t nowEpoch = time(nullptr);
    cache.hadArrivals = rowsHaveArrivals(parsed.rows);
    if (nowEpoch >= kClockSetEpoch) {
        pruneExpiredArrivals(parsed.rows, nowEpoch);
    }
    cache.stopCode = stop.code;
    cache.rows = parsed.rows;
    cache.label = label;
    cache.updatedAtEpoch = parsed.updatedAtEpoch;
    cache.fetchedAtMillis = millis();
    cache.notFound = false;
    cache.valid = true;
    resetBackoff(backoffState);

    if (!cache.hadArrivals) {
        renderCachedPage();
        return;
    }
    
    bool hasLabels = cachedRowsHaveLabels(cache.rows);
    if (currentPage >=
        servicePageCount(cache.rows, servicesPerScreen(hasLabels))) {
        currentPage = 0;
    }
    renderCachedPage();
}

void stepForward() {
    if (currentStopIndex >= kMaxBusStops) {
        return;
    }
    
    StopCache& cache = stopCaches[currentStopIndex];
    if (!cache.valid) {
        // No cache yet, move to next stop
        currentPage = 0;
        currentStopIndex = (currentStopIndex + 1) % busStops.size();
        needsImmediateFetch = true;
        return;
    }
    
    std::vector<BusServiceRow> live = cache.rows;
    if (time(nullptr) >= kClockSetEpoch) {
        pruneExpiredArrivals(live, time(nullptr));
    }
    bool hasLabels = cachedRowsHaveLabels(live);
    size_t totalPages =
        servicePageCount(live, servicesPerScreen(hasLabels));
    if (currentPage + 1 < totalPages) {
        ++currentPage;
        renderCachedPage();
        return;
    }
    currentPage = 0;
    currentStopIndex = (currentStopIndex + 1) % busStops.size();
    needsImmediateFetch = true;
}

void stepBack() {
    if (currentPage > 0) {
        --currentPage;
        renderCachedPage();
        return;
    }
    currentPage = 0;
    currentStopIndex =
        (currentStopIndex + busStops.size() - 1) % busStops.size();
    needsImmediateFetch = true;
}

void enterSleep() {
    displaySetDimmed(false);
    displayShowStatus("Sleeping...");
    delay(kSleepNoticeMs);
    displaySleep();

    configServerStopMdns();
    configServerStopCaptiveDns();
    wifiLinkPrepareSleep();

    hal::sleepUntilButtonPress();

    displayWake();
    displayShowStatus("Waking up...");

    wifiLinkOnWake();

    // Invalidate all stop caches after sleep
    for (StopCache& cache : stopCaches) {
        cache.valid = false;
        cache.rows.clear();
        cache.notFound = false;
        cache.hadArrivals = false;
    }
    currentPage = 0;
    noStopsRendered = false;
    offlineRendered = false;
    connectingRendered = false;
    inConfigScreen = false;
    needsImmediateFetch = true;
    powerMode = PowerMode::Awake;
    noteInteraction();
    ignoreSleepClickUntilMs = millis() + kIgnoreSleepClickAfterWakeMs;
}

bool applyPowerMode() {
    SleepSettings settings;
    settings.enabled = !alwaysOn;
    settings.dimAfterMs = kDimAfterMs;
    settings.sleepAfterMs = kSleepAfterMs;

    IdleInputs inputs;
    inputs.nowMs = millis();
    inputs.lastInteractionMs = lastInteractionMillis;
    inputs.externallyPowered = hal::powerStatus().externalPower;

    PowerMode next = nextPowerMode(settings, inputs);
    if (next == PowerMode::Asleep) {
        enterSleep();
        return true;
    }
    if (next != powerMode) {
        displaySetDimmed(next == PowerMode::Dimmed);
        powerMode = next;
    }
    return false;
}

void showConfigScreen() {
    uint32_t nowMs = millis();
    std::string ip = wifiLinkIp();
    displayShowConfig("busaunty.local", ip,
                      configServerUnlockRemainingMs(nowMs));
}

void openConfigScreen() {
    displayShowStatus("Unlocking...");
    delay(kPortalNoticeMs);
    configServerUnlock(millis());
    if (wifiLinkConnected()) {
        configServerStartMdns();
    }
    inConfigScreen = true;
    offlineRendered = false;
    showConfigScreen();
    noteInteraction();
}

void onLinkState(WifiLinkState next) {
    if (next == lastLinkState) {
        return;
    }
    if (next == WifiLinkState::Connected) {
        configServerStopCaptiveDns();
        configServerStartMdns();
        if (!timeSynced) {
            syncTime();
        }
        // Don't clear caches - keep last-good data, let 10-minute rule decide
        noStopsRendered = false;
        offlineRendered = false;
        connectingRendered = false;
        needsImmediateFetch = true;
        noteInteraction();
    } else if (lastLinkState == WifiLinkState::Connected) {
        configServerStopMdns();
        timeSynced = false;
        offlineRendered = false;
    }
    if (next != WifiLinkState::ApFallback) {
        configServerStopCaptiveDns();
    }
    lastLinkState = next;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    displaySetup();
    hal::buttonsBegin();
    hal::powerBegin();
    hal::setHoldThreshold(hal::Button::Primary, kSleepHoldMs);
    hal::setHoldThreshold(hal::Button::Secondary, kPortalHoldMs);

    busStops = loadBusStops();
    alwaysOn = loadAlwaysOn();
    win95Theme = loadWin95Theme();
    wifiNetworks = loadWifiNetworks();

    // displaySetup() has already run with the plain layout, because the panel
    // has to be alive before NVS is worth reading. This is where the saved
    // theme actually takes effect.
    displaySetTheme(win95Theme);

    logWifiDiagnostics();

    // lwIP's socket mutex does not exist until the WiFi driver is started.
    // WebServer::begin() (and esp_wifi_get_config) take that mutex, so mode
    // has to come up before either of them — including when we already have
    // saved networks and would otherwise skip straight to the config server.
    WiFi.mode(WIFI_STA);
    if (!wifiNetworksKeyExists()) {
        WifiNetwork imported;
        if (wifiLinkImportStaCredentials(&imported)) {
            wifiNetworks = {imported};
            saveWifiNetworks(wifiNetworks);
        }
    }

    ConfigServerData config;
    config.networks = &wifiNetworks;
    config.stops = &busStops;
    config.alwaysOn = &alwaysOn;
    config.win95Theme = &win95Theme;
    config.networksDirty = &networksDirty;
    config.stopsDirty = &stopsDirty;
    config.alwaysOnDirty = &alwaysOnDirty;
    config.win95ThemeDirty = &win95ThemeDirty;
    configServerBegin(config);

    wifiLinkBegin(wifiNetworks);
    lastLinkState = wifiLinkState();

    if (wifiNetworks.empty()) {
        startSetupAp();
        lastLinkState = wifiLinkState();
    } else {
        displayShowStatus("Connecting WiFi...");
    }
    noteInteraction();
}

void loop() {
    hal::buttonsUpdate();
    hal::powerPoll();
    wifiLinkTick(millis());
    configServerTick();
    persistDirtySettings();
    onLinkState(wifiLinkState());

    if (hal::wasPressed(hal::Button::Primary) ||
        hal::wasPressed(hal::Button::Previous) ||
        hal::wasPressed(hal::Button::Secondary) ||
        hal::wasPressed(hal::Button::Sleep)) {
        noteInteraction();
        if (powerMode != PowerMode::Awake) {
            displaySetDimmed(false);
            powerMode = PowerMode::Awake;
        }
        wifiLinkForceScan();
    }

    if (inConfigScreen) {
        if (!configServerIsUnlocked(millis()) ||
            hal::wasClicked(hal::Button::Primary)) {
            inConfigScreen = false;
            offlineRendered = false;
            noStopsRendered = false;
            needsImmediateFetch = wifiLinkConnected();
            noteInteraction();
            return;
        }
        if (hal::wasClicked(hal::Button::Secondary)) {
            startSetupAp();
            return;
        }
        showConfigScreen();
        delay(50);
        return;
    }

    if (hal::wasHold(hal::Button::Secondary)) {
        openConfigScreen();
        return;
    }

    if (hal::wasClicked(hal::Button::Sleep) &&
        static_cast<int32_t>(millis() - ignoreSleepClickUntilMs) >= 0) {
        enterSleep();
        return;
    }

    if (!hal::hasButton(hal::Button::Sleep) &&
        hal::wasHold(hal::Button::Primary)) {
        enterSleep();
        return;
    }

    // A connect after light sleep can take longer than the dim delay. The
    // idle clock still runs, but sleeping mid-associate just puts us back
    // here with the radio off again.
    if (!wifiLinkConnecting() && applyPowerMode()) {
        return;
    }

    if (wifiLinkState() == WifiLinkState::ApFallback) {
        delay(50);
        return;
    }

    const bool cachedStop =
        !busStops.empty() && currentStopIndex < busStops.size() &&
        currentStopIndex < kMaxBusStops &&
        stopCaches[currentStopIndex].valid;

    if (wifiLinkConnecting()) {
        if (cachedStop && keepArrivalsOnWifiLoss(true)) {
            renderCachedPage();
            connectingRendered = false;
            offlineRendered = false;
        } else if (!connectingRendered) {
            displayShowStatus("Connecting WiFi...");
            connectingRendered = true;
            offlineRendered = false;
        }
        delay(50);
        return;
    }

    if (!wifiLinkConnected()) {
        if (cachedStop &&
            keepArrivalsOnWifiLoss(stopCaches[currentStopIndex].valid)) {
            renderCachedPage();
            offlineRendered = false;
            connectingRendered = false;
        } else if (!offlineRendered && !inConfigScreen) {
            displayShowWifiOffline();
            offlineRendered = true;
            connectingRendered = false;
        }
        delay(50);
        return;
    }

    if (busStops.empty()) {
        if (!noStopsRendered) {
            displayShowNoStops();
            noStopsRendered = true;
        }
        delay(50);
        return;
    }

    if (hal::wasClicked(hal::Button::Primary)) {
        stepForward();
    }
    if (hal::wasClicked(hal::Button::Previous)) {
        stepBack();
    }

    uint32_t now = millis();
    
    // Refresh display periodically to update ETAs and stale age
    if (shouldRefreshDisplay(lastDisplayRefreshMs, now)) {
        renderCachedPage();
        lastDisplayRefreshMs = now;
    }
    
    // Attempt fetch based on backoff schedule
    if (needsImmediateFetch || shouldAttemptFetch(backoffState, now)) {
        pollAndRender();
        lastDisplayRefreshMs = now;
        needsImmediateFetch = false;
    }
}
