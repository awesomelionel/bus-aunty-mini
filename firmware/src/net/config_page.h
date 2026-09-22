// firmware/src/net/config_page.h
#pragma once
#include <WebServer.h>

#include <string>
#include <vector>

#include "core/bus_stop_config.h"
#include "core/wifi_credentials.h"

// Board-agnostic markup for the settings portal. Kept apart from
// config_server.cpp so routing and rendering stay separately readable: the
// markup is most of the bytes and almost none of the logic.
namespace config_page {

// Everything the page renders from. Pure input -- the page changes no state,
// so a handler can render the same model twice (once with an error) without
// worrying about what the first pass did.
struct Model {
    const std::vector<WifiNetwork>* networks = nullptr;
    const std::vector<BusStopConfig>* stops = nullptr;
    std::vector<std::string> visibleSsids;
    bool alwaysOn = false;
    bool win95Theme = false;
    // False on the 240x135 boards, where window chrome would leave two rows
    // out of six. The control is not rendered at all rather than shown and
    // ignored.
    bool supportsFramedTheme = false;
    // True while the device is serving its own setup AP, which is also the
    // only time tearing the radio down would disconnect the browser reading
    // this page.
    bool apMode = false;
    // Shown in a callout at the top. Empty when there is nothing to say.
    std::string error;
};

// Streamed in chunks rather than built into one String: the markup runs to
// several kilobytes and an ESP32 has 320KB of RAM to lose to heap
// fragmentation.
void sendSettings(WebServer& server, const Model& model);

// Sent *before* the radio switches, so the browser still has a connection to
// receive it on. After this the setup AP disappears from the phone.
void sendConnecting(WebServer& server, const std::string& ssid);

void sendLocked(WebServer& server);

}  // namespace config_page
