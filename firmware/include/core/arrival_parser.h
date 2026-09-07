#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct BusArrivalTimes {
    int64_t eta1Epoch = -1;
    int64_t eta2Epoch = -1;
    int64_t eta3Epoch = -1;
};

struct BusService {
    std::string serviceNo;
    BusArrivalTimes times;
};

struct ParsedBusStop {
    bool valid = false;
    std::string busStopCode;
    std::vector<BusService> services;
};

ParsedBusStop parseBusArrivalResponse(const std::string& json,
                                       const std::string& expectedStopCode);

// A stop can list more services than fit on screen, so they are shown a page
// at a time. Returns 0 pages when there is nothing to show.
size_t servicePageCount(size_t serviceCount, size_t pageSize);
std::vector<BusService> selectServicePage(
    const std::vector<BusService>& services, size_t pageSize, size_t page);
