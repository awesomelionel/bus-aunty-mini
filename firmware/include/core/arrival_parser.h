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

// The feed carries at most three upcoming buses per service. A row keeps that
// same cap after arrivals from several slots are merged.
constexpr size_t kArrivalsPerService = 3;

// A hostile or huge payload must not become thousands of rows. A busy
// interchange fits well inside both limits.
constexpr size_t kMaxServicesPerStop = 48;
constexpr size_t kMaxRowsPerStop = 96;

// Reject a response body larger than this before it is buffered. Real stops
// are a few kilobytes.
constexpr int kMaxArrivalBodyBytes = 48 * 1024;

// Drop an arrival once it is more than this far past its ETA. Exactly this
// many seconds past is still shown (as "Arr").
constexpr int64_t kEtaDropPastSeconds = 120;

struct BusArrival {
    int64_t etaEpoch = -1;
    BusLoad load = BusLoad::Unknown;
    BusType type = BusType::Unknown;
    std::string visitNumber;  // "1", "2", or empty
    // The bus ends its trip at this stop. Marked on screen, never hidden.
    bool terminating = false;
};

// A row is one (service, destination label, visit) on screen.
// `label` is the destination with a leading "To " removed. Empty when the
// feed had no label and the loop description did not fill in.
struct BusServiceRow {
    std::string serviceNo;
    std::string label;
    std::string visitNumber;
    bool isLoop = false;
    std::array<BusArrival, kArrivalsPerService> arrivals{};
};

// One Services[] entry, before rows are grouped.
struct BusService {
    std::string serviceNo;
    bool isLoop = false;
    std::string loopDesc;
    std::array<BusArrival, kArrivalsPerService> arrivals{};
    std::array<std::string, kArrivalsPerService> labels{};
};

struct ParsedBusStop {
    bool valid = false;
    std::string busStopCode;
    int64_t updatedAtEpoch = -1;
    std::vector<BusService> services;
    std::vector<BusServiceRow> rows;
};

ParsedBusStop parseBusArrivalResponse(const std::string& json,
                                       const std::string& expectedStopCode);

BusLoad parseBusLoad(const std::string& raw);
BusType parseBusType(const std::string& raw);

// Strip a leading "To ". Labels without that prefix are returned unchanged.
std::string stripToPrefix(const std::string& label);

// Group Services entries into rows keyed by (ServiceNo, Label, VisitNumber).
// Services stay in first-seen API order. Within a service the rows are
// visit "1" then visit "2", and within a visit the labels are alphabetical.
// That order does not follow v2 1.08's first-seen order: a lead bus leaving
// must not swap two directions. See arrival_parser.cpp.
std::vector<BusServiceRow> flattenToRows(const std::vector<BusService>& services);

// Remove arrivals more than kEtaDropPastSeconds in the past. A row that has
// nothing left is dropped. An unset clock (nowEpoch < kClockSetEpoch) is a
// no-op so the device stays on "Loading..." rather than deleting buses.
void pruneExpiredArrivals(std::vector<BusServiceRow>& rows, int64_t nowEpoch);

bool rowsHaveArrivals(const std::vector<BusServiceRow>& rows);

// Every timed arrival on the row is terminating. Empty slots do not count.
// An empty row is not "all terminating".
bool rowAllTerminating(const BusServiceRow& row);

size_t servicePageCount(const std::vector<BusServiceRow>& rows, size_t pageSize);
std::vector<BusServiceRow> selectServicePage(
    const std::vector<BusServiceRow>& rows, size_t pageSize, size_t page);

#ifdef ESP32
#include <Stream.h>
ParsedBusStop parseBusArrivalStream(Stream& input,
                                    const std::string& expectedStopCode);
#endif

// "2nd" sits on the label line only when the row has a destination (or a loop
// description standing in for one), every timed arrival is visit "2", and the
// row is not the all-terminating "Ends here" case.
bool shouldShowVisit2Marker(const BusServiceRow& row);

// True when the row draws a second line: a destination, a loop description,
// or "Ends here" for an all-terminating row.
inline bool rowShowsLabel(const BusServiceRow& row) {
    return !row.label.empty() || rowAllTerminating(row);
}
