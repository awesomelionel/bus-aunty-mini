#include "net/config_server.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <cstddef>
#include <cstdio>

#include "core/html_escape.h"
#include "core/wifi_policy.h"
#include "net/wifi_link.h"

namespace {

WebServer server(80);
DNSServer dns;
ConfigServerData data;
uint32_t unlockUntilMs = 0;
bool captiveDns = false;
bool mdnsUp = false;

void sendHtml(const String& body) {
    server.send(200, "text/html; charset=utf-8", body);
}

void sendLocked() {
    sendHtml(
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Bus Aunty</title></head><body>"
        "<p>Press the button on your device to unlock settings.</p>"
        "</body></html>");
}

bool requireUnlock() {
    if (configServerIsUnlocked(millis())) {
        return true;
    }
    sendLocked();
    return false;
}

void redirectHome() {
    server.sendHeader("Location", "/", true);
    server.send(303, "text/plain", "");
}

void markNetworksDirty() {
    if (data.networksDirty != nullptr) {
        *data.networksDirty = true;
    }
    wifiLinkSetNetworks(*data.networks);
}

String networkRows() {
    String html;
    const std::vector<WifiNetwork>& nets = *data.networks;
    for (size_t i = 0; i < nets.size(); ++i) {
        const char* placeholder = nets[i].password.empty()
                                      ? "open network — leave blank"
                                      : "saved — leave blank to keep";
        html += "<li><form method='post' action='/networks' accept-charset='utf-8'>";
        html += "<input type='hidden' name='index' value='";
        html += String(static_cast<unsigned>(i));
        html += "'>";
        html += "<strong>";
        html += escapeHtml(nets[i].ssid).c_str();
        html += "</strong> ";
        html += "<input type='password' name='password' value='' placeholder='";
        html += escapeHtml(placeholder).c_str();
        html += "'>";
        html += "<button name='action' value='save_pass'>Save</button>";
        if (i > 0) {
            html += "<button name='action' value='up'>Up</button>";
        }
        if (i + 1 < nets.size()) {
            html += "<button name='action' value='down'>Down</button>";
        }
        html += "<button name='action' value='delete'>Delete</button>";
        html += "</form></li>";
    }
    return html;
}

String scanOptions() {
    String html;
    html += "<option value=''>Choose a network</option>";
    for (const std::string& ssid : wifiLinkVisibleSsids()) {
        html += "<option value='";
        html += escapeHtml(ssid).c_str();
        html += "'>";
        html += escapeHtml(ssid).c_str();
        html += "</option>";
    }
    return html;
}

String stopFields() {
    String html;
    const std::vector<BusStopConfig>& stops = *data.stops;
    for (size_t i = 0; i < kMaxBusStops; ++i) {
        const char* code = i < stops.size() ? stops[i].code.c_str() : "";
        const char* name = i < stops.size() ? stops[i].name.c_str() : "";
        html += "<p>Stop ";
        html += String(static_cast<unsigned>(i + 1));
        html += " <input name='code";
        html += String(static_cast<unsigned>(i + 1));
        html += "' value='";
        html += escapeHtml(code).c_str();
        html += "' maxlength='5' inputmode='numeric' placeholder='00481'> ";
        html += "<input name='name";
        html += String(static_cast<unsigned>(i + 1));
        html += "' value='";
        html += escapeHtml(name).c_str();
        html += "' maxlength='16' placeholder='Home'></p>";
    }
    return html;
}

void handleRoot() {
    if (!requireUnlock()) {
        return;
    }
    String html;
    html +=
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Bus Aunty</title>"
        "<style>body{font-family:sans-serif;max-width:40em;margin:1em}"
        "li{margin:0.6em 0}button{margin-right:0.3em}</style>"
        "</head><body><h1>Bus Aunty</h1>";

    html += "<h2>WiFi networks</h2><p>Top of the list is tried first.</p><ol>";
    html += networkRows();
    html += "</ol>";
    html +=
        "<form method='post' action='/scan'><button>Rescan nearby "
        "networks</button></form>";
    html +=
        "<form method='post' action='/networks' accept-charset='utf-8'>"
        "<input type='hidden' name='action' value='add'>"
        "<p><select name='scan_ssid'>";
    html += scanOptions();
    html +=
        "</select></p>"
        "<p><input name='ssid' placeholder='or type SSID'></p>"
        "<p><input type='password' name='password' placeholder='password'></p>"
        "<p><button>Add network</button></p></form>";

    html +=
        "<h2>Bus stops</h2>"
        "<p>Up to 4 bus stops. The code is required: 3-5 digits, keeping any "
        "leading zeros (e.g. 00481). The name is optional and falls back to "
        "the code. A name with no code is ignored.</p>"
        "<form method='post' action='/stops' accept-charset='utf-8'>";
    html += stopFields();
    html += "<p><button>Save stops</button></p></form>";

    html +=
        "<h2>Always on</h2>"
        "<p>Tick this if the device stays plugged in. It cannot always tell "
        "mains power from a full battery, so left unticked a permanently "
        "powered device will still dim and sleep.</p>"
        "<form method='post' action='/alwayson'>"
        "<p><label><input type='checkbox' name='alwayson' value='T'";
    if (data.alwaysOn != nullptr && *data.alwaysOn) {
        html += " checked";
    }
    html +=
        "> Always on (skip dimming and sleep)</label></p>"
        "<p><button>Save</button></p></form>";
    html += "</body></html>";
    sendHtml(html);
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
                incoming[i].password = std::string(server.arg("password").c_str());
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
    redirectHome();
}

void handleAlwaysOn() {
    if (!requireUnlock()) {
        return;
    }
    *data.alwaysOn = server.hasArg("alwayson");
    if (data.alwaysOnDirty != nullptr) {
        *data.alwaysOnDirty = true;
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
    server.on("/alwayson", HTTP_POST, handleAlwaysOn);
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
