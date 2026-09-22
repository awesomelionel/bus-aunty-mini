#include "net/config_server.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <cstddef>
#include <cstdio>
#include <string>

#include "board/board.h"
#include "config_page.h"
#include "core/wifi_policy.h"
#include "net/wifi_link.h"

namespace {

WebServer server(80);
DNSServer dns;
ConfigServerData data;
uint32_t unlockUntilMs = 0;
bool captiveDns = false;
bool mdnsUp = false;

// Set by /apply and acted on after the response has gone out. Applying inline
// would disconnect the browser mid-request: wifiLinkSetNetworks() drops the
// setup AP, which is the very link the reply has to travel back over.
bool applyPending = false;

bool apMode() { return wifiLinkState() == WifiLinkState::ApFallback; }

void redirectHome() {
    server.sendHeader("Location", "/", true);
    server.send(303, "text/plain", "");
}

void renderSettings(const std::string& error) {
    config_page::Model model;
    model.networks = data.networks;
    model.stops = data.stops;
    model.visibleSsids = wifiLinkVisibleSsids();
    model.alwaysOn = data.alwaysOn != nullptr && *data.alwaysOn;
    model.win95Theme = data.win95Theme != nullptr && *data.win95Theme;
    model.supportsFramedTheme = board().supportsFramedTheme;
    model.apMode = apMode();
    model.error = error;
    config_page::sendSettings(server, model);
}

// While the setup AP is up, the AP's WPA2 password is already the gate --
// nobody reaches this server without it -- and a five-minute window that
// expires mid-setup just loses the user's typing. On the LAN the window still
// applies, but any request refreshes it, so a slow form fill cannot strand
// someone either.
bool requireUnlock() {
    if (apMode()) {
        return true;
    }
    if (configServerIsUnlocked(millis())) {
        configServerUnlock(millis());
        return true;
    }
    config_page::sendLocked(server);
    return false;
}

void markNetworksDirty() {
    if (data.networksDirty != nullptr) {
        *data.networksDirty = true;
    }
    // Deliberately does not call wifiLinkSetNetworks(). Edits are persisted by
    // the main loop and only take to the air when the user presses Save &
    // Connect, so adding a network cannot pull the ground out from under the
    // page that added it.
}

void parseStops() {
    std::vector<BusStopConfig> rows;
    rows.reserve(kMaxBusStops);
    for (size_t i = 0; i < kMaxBusStops; ++i) {
        char codeId[8];
        char nameId[8];
        std::snprintf(codeId, sizeof(codeId), "code%u",
                      static_cast<unsigned>(i + 1));
        std::snprintf(nameId, sizeof(nameId), "name%u",
                      static_cast<unsigned>(i + 1));
        rows.push_back({std::string(server.arg(codeId).c_str()),
                        std::string(server.arg(nameId).c_str())});
    }
    *data.stops = buildBusStopList(rows);
    if (data.stopsDirty != nullptr) {
        *data.stopsDirty = true;
    }
}

void handleRoot() {
    if (!requireUnlock()) {
        return;
    }
    renderSettings("");
}

void handleScan() {
    if (!requireUnlock()) {
        return;
    }
    wifiLinkRequestVisibleScan();
    redirectHome();
}

void handleNetworks() {
    if (!requireUnlock()) {
        return;
    }
    const String action = server.arg("action");
    std::vector<WifiNetwork>& nets = *data.networks;
    if (action == "add") {
        String typed = server.arg("ssid");
        typed.trim();
        String scanned = server.arg("scan_ssid");
        WifiNetwork row;
        row.ssid = typed.length() > 0 ? std::string(typed.c_str())
                                      : std::string(scanned.c_str());
        row.password = std::string(server.arg("password").c_str());
        if (row.ssid.empty()) {
            renderSettings("Pick a network from the list or type its name.");
            return;
        }
        std::vector<WifiNetwork> incoming = nets;
        incoming.push_back(row);
        nets = mergeWifiPasswords(nets, incoming);
        markNetworksDirty();
    } else {
        int index = server.arg("index").toInt();
        if (index >= 0 && static_cast<size_t>(index) < nets.size()) {
            size_t i = static_cast<size_t>(index);
            if (action == "delete") {
                nets.erase(nets.begin() + static_cast<std::ptrdiff_t>(i));
                markNetworksDirty();
            } else if (action == "up" && i > 0) {
                std::swap(nets[i], nets[i - 1]);
                markNetworksDirty();
            } else if (action == "down" && i + 1 < nets.size()) {
                std::swap(nets[i], nets[i + 1]);
                markNetworksDirty();
            } else if (action == "save_pass") {
                std::vector<WifiNetwork> incoming = nets;
                incoming[i].password =
                    std::string(server.arg("password").c_str());
                nets = mergeWifiPasswords(nets, incoming);
                markNetworksDirty();
            }
        }
    }
    redirectHome();
}

void handleStops() {
    if (!requireUnlock()) {
        return;
    }
    parseStops();
    redirectHome();
}

// Saves the stops on the way past, because the stop fields and this button are
// one form: pressing Connect can never commit the radio to a network while
// leaving the stops the user just typed behind.
void handleApply() {
    if (!requireUnlock()) {
        return;
    }
    parseStops();

    if (data.stops->empty()) {
        renderSettings(
            "Add at least one bus stop before connecting, or the device will "
            "join your WiFi with nothing to show.");
        return;
    }
    if (data.networks->empty()) {
        renderSettings("Add a WiFi network below before connecting.");
        return;
    }

    applyPending = true;
    config_page::sendConnecting(server, (*data.networks)[0].ssid);
}

// Both display settings arrive from one form with one Apply button, so this
// reads both every time: an unchecked box sends nothing, which is only
// distinguishable from "left alone" because the whole form posts together.
void handleDisplay() {
    if (!requireUnlock()) {
        return;
    }
    *data.alwaysOn = server.hasArg("alwayson");
    if (data.alwaysOnDirty != nullptr) {
        *data.alwaysOnDirty = true;
    }
    // Guarded by the board as well as by the page that drew the control: a
    // hand-made POST must not be able to put a 240x135 panel into a theme it
    // cannot show.
    if (data.win95Theme != nullptr && board().supportsFramedTheme) {
        const bool wanted = server.hasArg("win95");
        if (wanted != *data.win95Theme) {
            *data.win95Theme = wanted;
            if (data.win95ThemeDirty != nullptr) {
                *data.win95ThemeDirty = true;
            }
        }
    }
    redirectHome();
}

void handleCaptiveProbe() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

}  // namespace

void configServerBegin(const ConfigServerData& next) {
    data = next;
    // Binding a listen socket requires the tcpip task. WiFi.mode() is what
    // starts it; calling begin() first asserts in xQueueSemaphoreTake.
    WiFi.mode(WIFI_STA);
    server.on("/", HTTP_GET, handleRoot);
    server.on("/scan", HTTP_POST, handleScan);
    server.on("/networks", HTTP_POST, handleNetworks);
    server.on("/stops", HTTP_POST, handleStops);
    server.on("/apply", HTTP_POST, handleApply);
    server.on("/display", HTTP_POST, handleDisplay);
    server.on("/generate_204", HTTP_GET, handleCaptiveProbe);
    server.on("/hotspot-detect.html", HTTP_GET, handleCaptiveProbe);
    server.onNotFound(handleRoot);
    server.begin();
}

void configServerTick() {
    if (captiveDns) {
        dns.processNextRequest();
    }
    server.handleClient();

    if (applyPending) {
        applyPending = false;
        // handleClient() has written the reply; give the socket a moment to
        // drain before the radio it travelled over is reconfigured.
        delay(120);
        wifiLinkSetNetworks(*data.networks);
    }
}

void configServerUnlock(uint32_t nowMs) {
    unlockUntilMs = nowMs + kWifiUnlockWindowMs;
}

bool configServerIsUnlocked(uint32_t nowMs) {
    return static_cast<int32_t>(nowMs - unlockUntilMs) < 0;
}

uint32_t configServerUnlockRemainingMs(uint32_t nowMs) {
    if (!configServerIsUnlocked(nowMs)) {
        return 0;
    }
    return unlockUntilMs - nowMs;
}

void configServerStartCaptiveDns() {
    dns.start(53, "*", WiFi.softAPIP());
    captiveDns = true;
}

void configServerStopCaptiveDns() {
    if (!captiveDns) {
        return;
    }
    dns.stop();
    captiveDns = false;
}

void configServerStartMdns() {
    if (mdnsUp) {
        return;
    }
    if (MDNS.begin("busaunty")) {
        MDNS.addService("http", "tcp", 80);
        mdnsUp = true;
    }
}

void configServerStopMdns() {
    if (!mdnsUp) {
        return;
    }
    MDNS.end();
    mdnsUp = false;
}
