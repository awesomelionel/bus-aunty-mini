// firmware/include/core/bus_stop_config.h
#pragma once
#include <cstddef>
#include <string>
#include <vector>

constexpr size_t kMaxBusStops = 4;
constexpr size_t kBusStopCodeMinDigits = 3;
constexpr size_t kBusStopCodeMaxDigits = 5;
constexpr size_t kBusStopNameMaxChars = 16;

struct BusStopConfig {
    std::string code;
    std::string name;
};

// The name is optional, so fall back to the code when it is empty.
const std::string& busStopLabel(const BusStopConfig& stop);

// Codes keep their leading zeros ("00481"), so they stay strings rather than
// being parsed as integers. Returns false when the code is unusable.
bool normalizeBusStopCode(const std::string& raw, std::string* out);
std::string normalizeBusStopName(const std::string& raw);

// Normalizes each row, drops rows whose code is missing or invalid, and keeps
// at most kMaxBusStops. Rows arrive straight from the portal form fields.
std::vector<BusStopConfig> buildBusStopList(
    const std::vector<BusStopConfig>& rows);

std::string serializeBusStops(const std::vector<BusStopConfig>& stops);
std::vector<BusStopConfig> deserializeBusStops(const std::string& blob);
