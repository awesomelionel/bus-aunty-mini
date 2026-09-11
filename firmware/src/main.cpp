// firmware/src/main.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <time.h>

#include <string>
#include <vector>

#include "core/arrival_parser.h"
#include "core/bus_stop_config.h"
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

constexpr uint32_t kPollIntervalMs = 30000;
constexpr uint32_t kPortalHoldMs = 3000;
constexpr uint32_t kSleepHoldMs = 1500;
constexpr char kSetupApSsid[] = "BusAuntySetup";

constexpr uint32_t kDimAfterMs = 30000;
constexpr uint32_t kSleepAfterMs = 120000;
constexpr uint32_t kSleepNoticeMs = 700;
constexpr uint32_t kPortalNoticeMs = 900;

std::vector<BusStopConfig> busStops;
std::vector<WifiNetwork> wifiNetworks;
bool alwaysOn = false;
bool networksDirty = false;
bool stopsDirty = false;
bool alwaysOnDirty = false;
size_t currentStopIndex = 0;
uint32_t lastPollMillis = 0;
bool needsImmediateFetch = true;
bool noStopsRendered = false;
bool offlineRendered = false;
bool inConfigScreen = false;
bool timeSynced = false;
WifiLinkState lastLinkState = WifiLinkState::Backoff;

uint32_t lastInteractionMillis = 0;
PowerMode powerMode = PowerMode::Awake;

std::vector<BusService> cachedServices;
std::string cachedLabel;
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
    if (stopsDirty) {
        saveBusStops(busStops);
        stopsDirty = false;
    }
    if (alwaysOnDirty) {
        saveAlwaysOn(alwaysOn);
        alwaysOnDirty = false;
    }
    if (networksDirty) {
        saveWifiNetworks(wifiNetworks);
        networksDirty = false;
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
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    uint32_t startedAt = millis();
    while (now < 1700000000 && millis() - startedAt < 15000) {
        delay(250);
        now = time(nullptr);
    }
    timeSynced = now >= 1700000000;
}

void renderCachedPage() {
    size_t totalPages =
        servicePageCount(cachedServices.size(), servicesPerScreen());
    std::vector<BusService> page =
        selectServicePage(cachedServices, servicesPerScreen(), currentPage);
    displayShowArrivals(cachedLabel, page, time(nullptr), currentStopIndex,
                         busStops.size(), currentPage, totalPages,
                         hal::powerStatus());
}

void pollAndRender() {
    const BusStopConfig& stop = busStops[currentStopIndex];
    const std::string& label = busStopLabel(stop);
    displayShowStatus("Loading " + label + "...");

    cachedServices.clear();
    wifiLinkSetBusy(true);
    FetchResult fetch = fetchBusArrival(stop.code);
    wifiLinkSetBusy(false);
    if (!fetch.ok) {
        displayShowStatus(fetch.httpStatus == 404
                               ? "No data for " + label
                               : std::string("Fetch failed (") +
                                     std::to_string(fetch.httpStatus) + ")");
        return;
    }

    ParsedBusStop parsed = parseBusArrivalResponse(fetch.body, stop.code);
    if (!parsed.valid) {
        displayShowStatus("Bad response for " + label);
        return;
    }
    if (parsed.services.empty()) {
        displayShowStatus(label + ": no services");
        return;
    }

    cachedServices = parsed.services;
    cachedLabel = label;
    if (currentPage >=
        servicePageCount(cachedServices.size(), servicesPerScreen())) {
        currentPage = 0;
    }
    renderCachedPage();
}

void stepForward() {
    size_t totalPages =
        servicePageCount(cachedServices.size(), servicesPerScreen());
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

    cachedServices.clear();
    currentPage = 0;
    noStopsRendered = false;
    offlineRendered = false;
    inConfigScreen = false;
    needsImmediateFetch = true;
    lastPollMillis = millis();
    powerMode = PowerMode::Awake;
    noteInteraction();
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
        cachedServices.clear();
        currentPage = 0;
        noStopsRendered = false;
        offlineRendered = false;
        needsImmediateFetch = true;
        lastPollMillis = millis();
        noteInteraction();
    } else if (lastLinkState == WifiLinkState::Connected) {
        configServerStopMdns();
        timeSynced = false;
        cachedServices.clear();
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
    wifiNetworks = loadWifiNetworks();

    logWifiDiagnostics();

    if (!wifiNetworksKeyExists()) {
        WiFi.mode(WIFI_STA);
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
    config.networksDirty = &networksDirty;
    config.stopsDirty = &stopsDirty;
    config.alwaysOnDirty = &alwaysOnDirty;
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

    if (hal::wasClicked(hal::Button::Sleep)) {
        enterSleep();
        return;
    }

    if (!hal::hasButton(hal::Button::Sleep) &&
        hal::wasHold(hal::Button::Primary)) {
        enterSleep();
        return;
    }

    if (applyPowerMode()) {
        return;
    }

    if (wifiLinkState() == WifiLinkState::ApFallback) {
        delay(50);
        return;
    }

    if (!wifiLinkConnected()) {
        if (!offlineRendered && !inConfigScreen) {
            displayShowWifiOffline();
            offlineRendered = true;
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
    if (needsImmediateFetch || now - lastPollMillis >= kPollIntervalMs) {
        pollAndRender();
        lastPollMillis = now;
        needsImmediateFetch = false;
    }
}
