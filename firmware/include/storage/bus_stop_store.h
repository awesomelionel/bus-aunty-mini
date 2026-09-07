// firmware/include/storage/bus_stop_store.h
#pragma once
#include <vector>

#include "core/bus_stop_config.h"

// Bus stops survive reboots in NVS, so the portal only has to be visited once.
std::vector<BusStopConfig> loadBusStops();
bool saveBusStops(const std::vector<BusStopConfig>& stops);
