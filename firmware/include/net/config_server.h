// firmware/include/net/config_server.h
#pragma once
#include <cstdint>
#include <vector>

#include "core/bus_stop_config.h"
#include "core/wifi_credentials.h"

// The server mutates these in place and raises the dirty flags. Persistence
// stays with the caller, the same way the old portal handed values back.
struct ConfigServerData {
    std::vector<WifiNetwork>* networks = nullptr;
    std::vector<BusStopConfig>* stops = nullptr;
    bool* alwaysOn = nullptr;
    bool* networksDirty = nullptr;
    bool* stopsDirty = nullptr;
    bool* alwaysOnDirty = nullptr;
};

void configServerBegin(const ConfigServerData& data);
void configServerTick();
void configServerUnlock(uint32_t nowMs);
bool configServerIsUnlocked(uint32_t nowMs);
uint32_t configServerUnlockRemainingMs(uint32_t nowMs);
void configServerStartCaptiveDns();
void configServerStopCaptiveDns();
void configServerStartMdns();
void configServerStopMdns();
