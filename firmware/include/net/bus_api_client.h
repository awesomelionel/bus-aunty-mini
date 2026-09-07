// firmware/include/net/bus_api_client.h
#pragma once
#include <string>

struct FetchResult {
    bool ok = false;
    int httpStatus = 0;
    std::string body;
};

FetchResult fetchBusArrival(const std::string& busStopCode);
