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
#include "hal/buttons.h"
#include "hal/power.h"
#include "hal/sleep.h"
#include "net/bus_api_client.h"
#include "net/wifi_portal.h"
#include "storage/bus_stop_store.h"
#include "storage/device_settings.h"
#include "ui/display.h"

namespace {

constexpr uint32_t kPollIntervalMs = 30000;
constexpr uint32_t kPortalHoldMs = 3000;
constexpr uint32_t kSleepHoldMs = 1500;
constexpr char kSetupApSsid[] = "BusAuntySetup";

// Long enough to read the screen and page through a couple of stops without it
// dimming under you, short enough that a device left face-up on a table is not
// still lit and polling a minute later.
constexpr uint32_t kDimAfterMs = 30000;
constexpr uint32_t kSleepAfterMs = 120000;
// How long "Sleeping..." stays up, so the screen going black reads as
// deliberate rather than as a flat battery.
constexpr uint32_t kSleepNoticeMs = 700;

// Cold start after light sleep needs a scan + associate + DHCP, so this is
// longer than a typical boot reconnect. Falling through early would flash
// "WiFi lost" on every wake even when the AP is fine.
constexpr uint32_t kWakeReconnectTimeoutMs = 20000;

std::vector<BusStopConfig> busStops;
// Set in the portal for deployments that are permanently plugged in, where
// dimming and sleeping are a nuisance rather than a saving.
bool alwaysOn = false;
size_t currentStopIndex = 0;
uint32_t lastPollMillis = 0;
bool needsImmediateFetch = true;
bool noStopsRendered = false;

uint32_t lastInteractionMillis = 0;
PowerMode powerMode = PowerMode::Awake;

// The last fetch is kept so paging through a long service list re-renders
// locally instead of hitting the API again on every button press.
std::vector<BusService> cachedServices;
std::string cachedLabel;
size_t currentPage = 0;

void onPortalStarted() { displayShowWifiSetup(kSetupApSsid); }

// Diagnostic: the screen only ever shows a binary connected/not-connected, so
// these are the only way to tell a rejected password from an AP the radio
// never saw, or from a reboot that restarts setup() before the portal opens.
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

// Comparing the serialized form keeps NVS untouched when a portal visit left
// the stops alone, which is the common case on every boot.
void persistStopsIfChanged(const std::string& before) {
    if (serializeBusStops(busStops) != before) {
        saveBusStops(busStops);
    }
}

void persistAlwaysOnIfChanged(bool before) {
    if (alwaysOn != before) {
        saveAlwaysOn(alwaysOn);
    }
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

    // Drop the previous stop's services so Btn A cannot page through stale
    // data if this fetch fails.
    cachedServices.clear();

    FetchResult fetch = fetchBusArrival(stop.code);
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

void noteInteraction() { lastInteractionMillis = millis(); }

// Stepping walks the current stop's pages before moving on to the adjacent
// stop, in whichever direction, so a stop with more services than fit is
// fully reachable without a gesture of its own. Both wrap, so a short list
// stays a loop rather than a dead end.
//
// Landing on a new stop always starts at its first page: how many pages it
// has is not known until it has been fetched.
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

// Blanks the screen, drops the radio, and blocks until a button is pressed.
void enterSleep() {
    // Back to full brightness first: on the idle path the screen is already
    // dimmed, and the notice is the one thing here that has to be read.
    displaySetDimmed(false);
    displayShowStatus("Sleeping...");
    delay(kSleepNoticeMs);
    displaySleep();

    // ESP-IDF requires the WiFi driver to be stopped before light sleep: the
    // radio is powered down either way, and leaving the driver "started"
    // means the post-wake mode(WIFI_STA) is a no-op and reconnect never
    // recovers. Disconnect alone is not enough -- if it fails, the radio
    // would stay up -- so WIFI_OFF is forced afterwards. Credentials stay
    // in NVS; WiFi.begin() below reloads them.
    WiFi.disconnect(/*wifioff=*/false, /*eraseap=*/false);
    WiFi.mode(WIFI_OFF);

    hal::sleepUntilButtonPress();

    displayWake();
    displayShowStatus("Waking up...");

    // Full bring-up rather than reconnect(): after WIFI_OFF the station
    // interface has to be created again before esp_wifi_connect can work.
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startedAt < kWakeReconnectTimeoutMs) {
        delay(100);
    }
    Serial.printf("[wifi] wake reconnect %s after %lums (status %d)\n",
                  WiFi.status() == WL_CONNECTED ? "ok" : "failed",
                  static_cast<unsigned long>(millis() - startedAt),
                  static_cast<int>(WiFi.status()));

    // However long the device was away, the cached arrivals have expired, and
    // the screen was cleared before sleeping, so everything is redrawn.
    cachedServices.clear();
    currentPage = 0;
    noStopsRendered = false;
    needsImmediateFetch = true;
    lastPollMillis = millis();
    powerMode = PowerMode::Awake;
    noteInteraction();
}

// Returns true when the device slept, so the caller can drop the rest of the
// iteration rather than act on button state read minutes ago.
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

// Reopens the captive portal so stops can be edited after the initial setup.
void openConfigPortal() {
    std::string beforeStops = serializeBusStops(busStops);
    bool beforeAlwaysOn = alwaysOn;
    wifiPortalReconfigure(kSetupApSsid, &busStops, &alwaysOn, onPortalStarted);
    persistStopsIfChanged(beforeStops);
    persistAlwaysOnIfChanged(beforeAlwaysOn);

    if (currentStopIndex >= busStops.size()) {
        currentStopIndex = 0;
    }
    cachedServices.clear();
    currentPage = 0;
    noStopsRendered = false;
    needsImmediateFetch = true;
    lastPollMillis = millis();
    noteInteraction();
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
    std::string savedStops = serializeBusStops(busStops);
    bool savedAlwaysOn = alwaysOn;

    logWifiDiagnostics();

    displayShowStatus("Connecting WiFi...");
    if (!wifiPortalConnect(kSetupApSsid, &busStops, &alwaysOn,
                           onPortalStarted)) {
        displayShowStatus("WiFi setup timed out.\nRestarting...");
        delay(3000);
        ESP.restart();
    }
    persistStopsIfChanged(savedStops);
    persistAlwaysOnIfChanged(savedAlwaysOn);

    syncTime();
    // Start the idle clock once the device is actually usable: WiFi setup and
    // the NTP sync can take longer than the dim delay on their own.
    noteInteraction();
}

void loop() {
    hal::buttonsUpdate();
    // Sampled out here rather than at render time: the loop is idle between
    // fetches, which is when the battery voltage reads true.
    hal::powerPoll();

    // Any press counts as use, whichever action it turns out to be, and takes
    // the backlight straight back up so the screen responds before the button
    // is even released.
    if (hal::wasPressed(hal::Button::Primary) ||
        hal::wasPressed(hal::Button::Previous) ||
        hal::wasPressed(hal::Button::Secondary) ||
        hal::wasPressed(hal::Button::Sleep)) {
        noteInteraction();
        if (powerMode != PowerMode::Awake) {
            displaySetDimmed(false);
            powerMode = PowerMode::Awake;
        }
    }

    if (hal::wasHold(hal::Button::Secondary)) {
        openConfigPortal();
        return;
    }

    // Clicked rather than pressed: the sleep button may share a physical
    // button with the portal's, and firing on the press would sleep the
    // device the moment someone started holding it for the portal.
    if (hal::wasClicked(hal::Button::Sleep)) {
        enterSleep();
        return;
    }

    // Boards with no button to spare for sleeping hold the primary one
    // instead, which is why it is a hold: it must not collide with paging.
    if (!hal::hasButton(hal::Button::Sleep) &&
        hal::wasHold(hal::Button::Primary)) {
        enterSleep();
        return;
    }

    if (applyPowerMode()) {
        return;
    }

    if (busStops.empty()) {
        if (!noStopsRendered) {
            displayShowNoStops(kSetupApSsid);
            noStopsRendered = true;
        }
        delay(50);
        return;
    }

    // Read on release, so a hold is never also a step.
    if (hal::wasClicked(hal::Button::Primary)) {
        stepForward();
    }
    if (hal::wasClicked(hal::Button::Previous)) {
        stepBack();
    }

    if (WiFi.status() != WL_CONNECTED) {
        displayShowStatus("WiFi lost, reconnecting...");
        // reconnect() only works when the station interface is still up;
        // after a failed wake bring-up it isn't, so fall back to a full
        // begin() with the credentials still in NVS.
        if (!WiFi.reconnect()) {
            WiFi.mode(WIFI_STA);
            WiFi.begin();
        }
        delay(1000);
        return;
    }

    uint32_t now = millis();
    if (needsImmediateFetch || now - lastPollMillis >= kPollIntervalMs) {
        pollAndRender();
        lastPollMillis = now;
        needsImmediateFetch = false;
    }
}
