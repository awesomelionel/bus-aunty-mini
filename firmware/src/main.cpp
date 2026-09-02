// firmware/src/main.cpp
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include <string>
#include <vector>

#include "arrival_parser.h"
#include "bus_stops.h"
#include "display.h"

namespace {

size_t currentStopIndex = 0;

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

void renderDemo(size_t stopIndex) {
    std::vector<BusService> demoServices;
    const char* demoNumbers[] = {"12", "147", "36", "980", "5", "88"};
    for (int i = 0; i < 6; ++i) {
        BusService svc;
        svc.serviceNo = demoNumbers[i];
        svc.times.eta1Epoch = time(nullptr) + (i + 1) * 90;
        svc.times.eta2Epoch = time(nullptr) + (i + 1) * 300;
        svc.times.eta3Epoch = time(nullptr) + (i + 1) * 600;
        demoServices.push_back(svc);
    }
    displayShowArrivals(kBusStopCodes[stopIndex], demoServices, time(nullptr),
                         stopIndex, kBusStopCodes.size());
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
    renderDemo(currentStopIndex);
}

void loop() {
    M5.update();
    if (M5.BtnA.wasPressed()) {
        currentStopIndex = (currentStopIndex + 1) % kBusStopCodes.size();
        renderDemo(currentStopIndex);
    }
}
