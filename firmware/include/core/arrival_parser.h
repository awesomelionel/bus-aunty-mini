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

std::vector<BusService> selectDisplayServices(
    const std::vector<BusService>& services, size_t maxCount);
