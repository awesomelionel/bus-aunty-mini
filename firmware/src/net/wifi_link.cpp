#include "net/wifi_link.h"

#include <WiFi.h>
#include <esp_wifi.h>

#include <cstring>

#include "core/wifi_policy.h"

namespace {

WifiLinkState state = WifiLinkState::Backoff;
std::vector<WifiNetwork> networks;
std::string lastSsid;
std::string lastPass;
std::string currentSsid;
std::vector<std::string> visible;
std::vector<size_t> candidates;
size_t candidateIndex = 0;
uint32_t failedCycles = 0;
uint32_t backoffEndsAt = 0;
uint32_t actionStartedAt = 0;
uint32_t lastUpgradeCheck = 0;
bool scanInProgress = false;
bool visibleScanOnly = false;
bool beginIssued = false;
bool busy = false;
bool wakeAttempt = false;

const char* stateName(WifiLinkState s) {
    switch (s) {
        case WifiLinkState::Scanning:
            return "scanning";
        case WifiLinkState::FastPath:
            return "fastpath";
        case WifiLinkState::Connecting:
            return "connecting";
        case WifiLinkState::Connected:
            return "connected";
        case WifiLinkState::Backoff:
            return "backoff";
        case WifiLinkState::ApFallback:
            return "ap";
    }
    return "?";
}

void enter(WifiLinkState next, uint32_t nowMs) {
    if (state != next) {
        Serial.printf("[wifi] state %s\n", stateName(next));
    }
    state = next;
    beginIssued = false;
    if (next != WifiLinkState::Scanning && next != WifiLinkState::Connected) {
        scanInProgress = false;
        visibleScanOnly = false;
    }
    if (next == WifiLinkState::Backoff) {
        backoffEndsAt = nowMs + wifiBackoffMs(failedCycles);
    }
    if (next == WifiLinkState::Scanning || next == WifiLinkState::FastPath ||
        next == WifiLinkState::Connecting) {
        actionStartedAt = nowMs;
        visibleScanOnly = false;
    }
    if (next == WifiLinkState::Connected) {
        lastUpgradeCheck = nowMs;
        failedCycles = 0;
    }
}

void startScan() {
    if (state == WifiLinkState::ApFallback) {
        WiFi.mode(WIFI_AP_STA);
    } else {
        WiFi.mode(WIFI_STA);
    }
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
    scanInProgress = true;
}

void collectScan() {
    int n = WiFi.scanComplete();
    visible.clear();
    if (n > 0) {
        visible.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            visible.push_back(std::string(WiFi.SSID(i).c_str()));
        }
    }
    WiFi.scanDelete();
    scanInProgress = false;
    visibleScanOnly = false;
}

void startConnect(const WifiNetwork& net, uint32_t nowMs) {
    currentSsid = net.ssid;
    WiFi.mode(WIFI_STA);
    if (net.password.empty()) {
        WiFi.begin(net.ssid.c_str());
    } else {
        WiFi.begin(net.ssid.c_str(), net.password.c_str());
    }
    beginIssued = true;
    actionStartedAt = nowMs;
    Serial.printf("[wifi] connecting %s\n", net.ssid.c_str());
}

void rememberCurrent() {
    lastSsid = currentSsid;
    for (const WifiNetwork& net : networks) {
        if (net.ssid == currentSsid) {
            lastPass = net.password;
            return;
        }
    }
}

void finishFailedCycle(uint32_t nowMs) {
    if (failedCycles < 1000) {
        ++failedCycles;
    }
    enter(WifiLinkState::Backoff, nowMs);
}

void handleScanComplete(uint32_t nowMs) {
    const bool onlyVisible = visibleScanOnly || state == WifiLinkState::Connected;
    collectScan();
    if (onlyVisible) {
        if (state == WifiLinkState::Connected &&
            shouldUpgradeWifi(networks, currentSsid, visible)) {
            candidates = selectWifiCandidates(networks, visible);
            if (!candidates.empty()) {
                WiFi.disconnect(/*wifioff=*/false, /*eraseap=*/false);
                candidateIndex = 0;
                enter(WifiLinkState::Connecting, nowMs);
            }
        }
        return;
    }
    candidates = selectWifiCandidates(networks, visible);
    if (candidates.empty()) {
        finishFailedCycle(nowMs);
        return;
    }
    candidateIndex = 0;
    enter(WifiLinkState::Connecting, nowMs);
}

void tickScanning(uint32_t nowMs) {
    if (!scanInProgress) {
        startScan();
        return;
    }
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
        return;
    }
    handleScanComplete(nowMs);
}

void tickConnecting(uint32_t nowMs) {
    if (!beginIssued) {
        if (candidateIndex >= candidates.size()) {
            finishFailedCycle(nowMs);
            return;
        }
        startConnect(networks[candidates[candidateIndex]], nowMs);
        return;
    }
    if (WiFi.status() == WL_CONNECTED) {
        rememberCurrent();
        enter(WifiLinkState::Connected, nowMs);
        return;
    }
    if (nowMs - actionStartedAt < kWifiConnectTimeoutMs) {
        return;
    }
    ++candidateIndex;
    beginIssued = false;
    if (candidateIndex >= candidates.size()) {
        finishFailedCycle(nowMs);
    }
}

void tickFastPath(uint32_t nowMs) {
    if (!beginIssued) {
        WifiNetwork net;
        net.ssid = lastSsid;
        net.password = lastPass;
        startConnect(net, nowMs);
        return;
    }
    if (WiFi.status() == WL_CONNECTED) {
        rememberCurrent();
        wakeAttempt = false;
        enter(WifiLinkState::Connected, nowMs);
        return;
    }
    const uint32_t timeoutMs =
        wakeAttempt ? kWifiWakeConnectTimeoutMs : kWifiConnectTimeoutMs;
    if (nowMs - actionStartedAt >= timeoutMs) {
        wakeAttempt = false;
        enter(WifiLinkState::Scanning, nowMs);
    }
}

void tickConnected(uint32_t nowMs) {
    if (WiFi.status() != WL_CONNECTED) {
        enter(WifiLinkState::Scanning, nowMs);
        return;
    }
    if (scanInProgress) {
        int n = WiFi.scanComplete();
        if (n != WIFI_SCAN_RUNNING) {
            handleScanComplete(nowMs);
        }
        return;
    }
    if (busy || networks.empty() || currentSsid == networks[0].ssid) {
        return;
    }
    if (nowMs - lastUpgradeCheck < kWifiUpgradeIntervalMs) {
        return;
    }
    lastUpgradeCheck = nowMs;
    visibleScanOnly = true;
    startScan();
}

void finishVisibleScanIfReady() {
    if (!scanInProgress) {
        return;
    }
    int n = WiFi.scanComplete();
    if (n != WIFI_SCAN_RUNNING) {
        collectScan();
    }
}

void tickBackoff(uint32_t nowMs) {
    finishVisibleScanIfReady();
    if (networks.empty()) {
        return;
    }
    if (nowMs >= backoffEndsAt) {
        enter(WifiLinkState::Scanning, nowMs);
    }
}

}  // namespace

void wifiLinkBegin(const std::vector<WifiNetwork>& saved) {
    networks = saved;
    failedCycles = 0;
    if (networks.empty()) {
        enter(WifiLinkState::Backoff, millis());
        return;
    }
    enter(WifiLinkState::Scanning, millis());
}

void wifiLinkSetNetworks(const std::vector<WifiNetwork>& saved) {
    networks = saved;
    uint32_t nowMs = millis();
    if (networks.empty()) {
        if (state != WifiLinkState::ApFallback) {
            enter(WifiLinkState::Backoff, nowMs);
        }
        return;
    }
    if (state == WifiLinkState::ApFallback) {
        WiFi.softAPdisconnect(/*wifioff=*/true);
    }
    enter(WifiLinkState::Scanning, nowMs);
}

void wifiLinkTick(uint32_t nowMs) {
    switch (state) {
        case WifiLinkState::Scanning:
            tickScanning(nowMs);
            break;
        case WifiLinkState::FastPath:
            tickFastPath(nowMs);
            break;
        case WifiLinkState::Connecting:
            tickConnecting(nowMs);
            break;
        case WifiLinkState::Connected:
            tickConnected(nowMs);
            break;
        case WifiLinkState::Backoff:
            tickBackoff(nowMs);
            break;
        case WifiLinkState::ApFallback:
            finishVisibleScanIfReady();
            break;
    }
}

void wifiLinkForceScan() {
    if (state != WifiLinkState::Backoff) {
        return;
    }
    if (networks.empty()) {
        return;
    }
    failedCycles = 0;
    enter(WifiLinkState::Scanning, millis());
}

void wifiLinkStartAp(const char* ssid, const char* password) {
    WiFi.scanDelete();
    scanInProgress = false;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    enter(WifiLinkState::ApFallback, millis());
    Serial.printf("[wifi] ap %s\n", ssid);
}

void wifiLinkStopAp() {
    WiFi.softAPdisconnect(/*wifioff=*/true);
    uint32_t nowMs = millis();
    if (networks.empty()) {
        enter(WifiLinkState::Backoff, nowMs);
        return;
    }
    enter(WifiLinkState::Scanning, nowMs);
}

void wifiLinkPrepareSleep() {
    WiFi.disconnect(/*wifioff=*/false, /*eraseap=*/false);
    WiFi.mode(WIFI_OFF);
}

void wifiLinkOnWake() {
    WiFi.mode(WIFI_STA);
    uint32_t nowMs = millis();
    wakeAttempt = true;
    if (lastSsid.empty() && !networks.empty()) {
        lastSsid = networks[0].ssid;
        lastPass = networks[0].password;
    }
    if (!lastSsid.empty()) {
        enter(WifiLinkState::FastPath, nowMs);
        return;
    }
    if (networks.empty()) {
        enter(WifiLinkState::Backoff, nowMs);
        return;
    }
    enter(WifiLinkState::Scanning, nowMs);
}

void wifiLinkSetBusy(bool next) { busy = next; }

void wifiLinkRequestVisibleScan() {
    if (scanInProgress) {
        return;
    }
    visibleScanOnly = true;
    startScan();
}

std::vector<std::string> wifiLinkVisibleSsids() { return visible; }

bool wifiLinkImportStaCredentials(WifiNetwork* out) {
    if (out == nullptr) {
        return false;
    }
    wifi_config_t cfg = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &cfg) != ESP_OK) {
        return false;
    }
    if (cfg.sta.ssid[0] == 0) {
        return false;
    }
    char ssid[33] = {};
    std::memcpy(ssid, cfg.sta.ssid, 32);
    char pass[65] = {};
    std::memcpy(pass, cfg.sta.password, 64);
    out->ssid = ssid;
    out->password = pass;
    return validateWifiSsid(out->ssid) && validateWifiPassword(out->password);
}

WifiLinkState wifiLinkState() { return state; }

bool wifiLinkConnected() {
    return state == WifiLinkState::Connected && WiFi.status() == WL_CONNECTED;
}

bool wifiLinkConnecting() {
    return state == WifiLinkState::FastPath ||
           state == WifiLinkState::Scanning ||
           state == WifiLinkState::Connecting;
}

std::string wifiLinkCurrentSsid() { return currentSsid; }

std::string wifiLinkIp() {
    if (state == WifiLinkState::ApFallback) {
        return std::string(WiFi.softAPIP().toString().c_str());
    }
    return std::string(WiFi.localIP().toString().c_str());
}

uint32_t wifiLinkBackoffEndsAt() { return backoffEndsAt; }
