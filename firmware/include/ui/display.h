// firmware/include/ui/display.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/arrival_parser.h"

void displaySetup();
void displayShowStatus(const std::string& message);
void displayShowWifiSetup(const std::string& ssid);
void displayShowNoStops(const std::string& ssid);
// `stopLabel` is the stop's name, or its code when it was left unnamed.
void displayShowArrivals(const std::string& stopLabel,
                          const std::vector<BusService>& services,
                          int64_t nowEpoch, size_t currentStopIndex,
                          size_t totalStops);
