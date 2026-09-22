#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// How full a bus is, from the feed's "Load" field. The display colours each
// arrival by this, so an unreadable or absent value has to stay distinct from
// a genuinely empty bus rather than defaulting to one.
enum class BusLoad {
    Unknown,
    SeatsAvailable,     // "SEA"
    StandingAvailable,  // "SDA"
    LimitedStanding,    // "LSD"
};

// Which vehicle the operator has put on this run, from the feed's "Type".
// Carried per arrival rather than per service, because it is: the next bus on
// a service can be a double decker and the one after it a single.
enum class BusType {
    Unknown,
    SingleDeck,  // "SD"
    DoubleDeck,  // "DD"
    Bendy,       // "BD"
};

// The feed carries at most three upcoming buses per service.
constexpr size_t kArrivalsPerService = 3;

struct BusArrival {
    int64_t etaEpoch = -1;
    BusLoad load = BusLoad::Unknown;
    BusType type = BusType::Unknown;
};

struct BusService {
    std::string serviceNo;
    std::array<BusArrival, kArrivalsPerService> arrivals;
};

struct ParsedBusStop {
    bool valid = false;
    std::string busStopCode;
    std::vector<BusService> services;
};

ParsedBusStop parseBusArrivalResponse(const std::string& json,
                                       const std::string& expectedStopCode);

BusLoad parseBusLoad(const std::string& raw);
BusType parseBusType(const std::string& raw);

// A stop can list more services than fit on screen, so they are shown a page
// at a time. Returns 0 pages when there is nothing to show.
size_t servicePageCount(size_t serviceCount, size_t pageSize);
std::vector<BusService> selectServicePage(
    const std::vector<BusService>& services, size_t pageSize, size_t page);
