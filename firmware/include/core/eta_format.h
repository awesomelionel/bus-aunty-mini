#pragma once
#include <cstdint>
#include <string>

// Shown when a bus is arriving now. Named so the display can highlight it
// without hardcoding the same string twice.
inline constexpr char kEtaArrivingLabel[] = "Arr";

std::string formatEtaMinutes(int64_t targetEpoch, int64_t nowEpoch);
