// firmware/include/ui/display.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/arrival_parser.h"
#include "ui/battery.h"

// How many service rows fit under the header at the arrivals font size.
constexpr size_t kServicesPerScreen = 6;

void displaySetup();
// Blanks the panel and puts it in its own low-power state. Nothing drawn while
// asleep reaches the screen, so callers should not bother drawing.
void displaySleep();
void displayWake();
void displayShowStatus(const std::string& message);
void displayShowWifiSetup(const std::string& ssid);
void displayShowNoStops(const std::string& ssid);
// `stopLabel` is the stop's name, or its code when it was left unnamed.
// `services` is already the slice for `currentPage`.
void displayShowArrivals(const std::string& stopLabel,
                          const std::vector<BusService>& services,
                          int64_t nowEpoch, size_t currentStopIndex,
                          size_t totalStops, size_t currentPage,
                          size_t totalPages, const BatteryReading& battery);
