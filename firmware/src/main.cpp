// firmware/src/main.cpp
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include <string>

#include "arrival_parser.h"
#include "bus_api_client.h"
#include "bus_stops.h"
#include "display.h"

namespace {

constexpr uint32_t kPollIntervalMs = 30000;

size_t currentStopIndex = 0;
uint32_t lastPollMillis = 0;
bool needsImmediateFetch = true;

void onEnterConfigPortal(WiFiManager* wm) {
    displayShowStatus("Connect WiFi to:\nBusAuntyDisplay-Setup");
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

void pollAndRender() {
    const char* code = kBusStopCodes[currentStopIndex];
    displayShowStatus(std::string("Loading ") + code + "...");

    FetchResult fetch = fetchBusArrival(code);
    if (!fetch.ok) {
        displayShowStatus(fetch.httpStatus == 404
                               ? std::string("No data for ") + code
                               : std::string("Fetch failed (") +
                                     std::to_string(fetch.httpStatus) + ")");
        return;
    }

    ParsedBusStop parsed = parseBusArrivalResponse(fetch.body, code);
    if (!parsed.valid) {
        displayShowStatus(std::string("Bad response for ") + code);
        return;
    }

    std::vector<BusService> shown = selectDisplayServices(parsed.services, 6);
    displayShowArrivals(parsed.busStopCode, shown, time(nullptr),
                         currentStopIndex, kBusStopCodes.size());
}

}  // namespace

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    displaySetup();

    WiFiManager wm;
    wm.setAPCallback(onEnterConfigPortal);
    wm.setConfigPortalTimeout(180);
    displayShowStatus("Connecting WiFi...");
    if (!wm.autoConnect("BusAuntyDisplay-Setup")) {
        displayShowStatus("WiFi setup timed out.\nRestarting...");
        delay(3000);
        ESP.restart();
    }

    syncTime();
}

void loop() {
    M5.update();

    if (M5.BtnA.wasPressed()) {
        currentStopIndex = (currentStopIndex + 1) % kBusStopCodes.size();
        needsImmediateFetch = true;
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
