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
    std::string visitNumber;  // "1", "2", or empty
};

// A row groups arrivals with the same (serviceNo, label) pair.
// The label is the destination stripped of "To " prefix.
struct BusServiceRow {
    std::string serviceNo;
    std::string label;  // Empty for services with only one direction at this stop
    bool isLoop = false;
    std::array<BusArrival, kArrivalsPerService> arrivals;
};

// Legacy structure for parsing from v2 API, before flattening.
struct BusService {
    std::string serviceNo;
    bool isLoop = false;
    std::array<BusArrival, kArrivalsPerService> arrivals;
    std::array<std::string, kArrivalsPerService> labels;
};

struct ParsedBusStop {
    bool valid = false;
    std::string busStopCode;
    std::vector<BusService> services;
    std::vector<BusServiceRow> rows;  // Flattened view grouped by (serviceNo, label)
};

ParsedBusStop parseBusArrivalResponse(const std::string& json,
                                       const std::string& expectedStopCode);

BusLoad parseBusLoad(const std::string& raw);
BusType parseBusType(const std::string& raw);

// Strip "To " prefix from a label. Returns empty string if input is empty.
std::string stripToPrefix(const std::string& label);

// Flatten Services entries into rows keyed by (serviceNo, label).
// Arrivals are sorted by ETA within each row, empty slots skipped.
// For loops, VisitNumber "1" rows come before "2" rows.
std::vector<BusServiceRow> flattenToRows(const std::vector<BusService>& services);

// A stop can list more rows than fit on screen, so they are shown a page
// at a time. Returns 0 pages when there is nothing to show.
size_t servicePageCount(size_t serviceCount, size_t pageSize);
size_t servicePageCount(const std::vector<BusServiceRow>& rows, size_t pageSize);
std::vector<BusServiceRow> selectServicePage(
    const std::vector<BusServiceRow>& rows, size_t pageSize, size_t page);
// Legacy overload for old tests
std::vector<BusService> selectServicePage(
    const std::vector<BusService>& services, size_t pageSize, size_t page);

// Returns true if row should show "2nd" marker
// (all non-empty arrivals are visit "2")
bool shouldShowVisit2Marker(const BusServiceRow& row);
