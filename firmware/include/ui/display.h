// firmware/include/ui/display.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/arrival_parser.h"

void displaySetup();
void displayShowStatus(const std::string& message);
void displayShowArrivals(const std::string& busStopCode,
                          const std::vector<BusService>& services,
                          int64_t nowEpoch, size_t currentStopIndex,
                          size_t totalStops);
