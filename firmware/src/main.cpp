#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

namespace {

void showStatus(const char* line1, const char* line2 = "") {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(4, 4);
    M5.Display.println(line1);
    if (line2[0] != '\0') {
        M5.Display.println(line2);
    }
}

void onEnterConfigPortal(WiFiManager* wm) {
    showStatus("Connect WiFi to:", "BusAuntyDisplay-Setup");
}

void syncTime() {
    showStatus("Syncing time...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    uint32_t startedAt = millis();
    while (now < 1700000000 && millis() - startedAt < 15000) {
        delay(250);
        now = time(nullptr);
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    WiFiManager wm;
    wm.setAPCallback(onEnterConfigPortal);
    wm.setConfigPortalTimeout(180);
    showStatus("Connecting WiFi...");
    if (!wm.autoConnect("BusAuntyDisplay-Setup")) {
        showStatus("WiFi setup timed out.", "Restarting...");
        delay(3000);
        ESP.restart();
    }

    syncTime();

    time_t now = time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmtime(&now));
    showStatus("Time synced (UTC):", buf);
}

void loop() {
    M5.update();
}
