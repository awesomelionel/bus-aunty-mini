// firmware/src/main.cpp
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_system.h>
#include <time.h>

#include <string>
#include <vector>

#include "core/arrival_parser.h"
#include "core/bus_stop_config.h"
#include "net/bus_api_client.h"
#include "net/wifi_portal.h"
#include "storage/bus_stop_store.h"
#include "ui/display.h"

namespace {

constexpr uint32_t kPollIntervalMs = 30000;
constexpr uint32_t kPortalHoldMs = 3000;
constexpr char kSetupApSsid[] = "BusAuntySetup";

std::vector<BusStopConfig> busStops;
size_t currentStopIndex = 0;
uint32_t lastPollMillis = 0;
bool needsImmediateFetch = true;
bool noStopsRendered = false;

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
        servicePageCount(cachedServices.size(), kServicesPerScreen);
    std::vector<BusService> page =
        selectServicePage(cachedServices, kServicesPerScreen, currentPage);
    displayShowArrivals(cachedLabel, page, time(nullptr), currentStopIndex,
                         busStops.size(), currentPage, totalPages);
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
        servicePageCount(cachedServices.size(), kServicesPerScreen)) {
        currentPage = 0;
    }
    renderCachedPage();
}

// Reopens the captive portal so stops can be edited after the initial setup.
void openConfigPortal() {
    std::string before = serializeBusStops(busStops);
    wifiPortalReconfigure(kSetupApSsid, &busStops, onPortalStarted);
    persistStopsIfChanged(before);

    if (currentStopIndex >= busStops.size()) {
        currentStopIndex = 0;
    }
    cachedServices.clear();
    currentPage = 0;
    noStopsRendered = false;
    needsImmediateFetch = true;
    lastPollMillis = millis();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.BtnB.setHoldThresh(kPortalHoldMs);
    displaySetup();

    busStops = loadBusStops();
    std::string savedStops = serializeBusStops(busStops);

    logWifiDiagnostics();

    displayShowStatus("Connecting WiFi...");
    if (!wifiPortalConnect(kSetupApSsid, &busStops, onPortalStarted)) {
        displayShowStatus("WiFi setup timed out.\nRestarting...");
        delay(3000);
        ESP.restart();
    }
    persistStopsIfChanged(savedStops);

    syncTime();
}

void loop() {
    M5.update();

    if (M5.BtnB.wasHold()) {
        openConfigPortal();
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

    // Btn A walks the current stop's remaining pages before moving on.
    if (M5.BtnA.wasPressed()) {
        size_t totalPages =
            servicePageCount(cachedServices.size(), kServicesPerScreen);
        if (currentPage + 1 < totalPages) {
            ++currentPage;
            renderCachedPage();
        } else {
            currentPage = 0;
            currentStopIndex = (currentStopIndex + 1) % busStops.size();
            needsImmediateFetch = true;
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        displayShowStatus("WiFi lost, reconnecting...");
        WiFi.reconnect();
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
