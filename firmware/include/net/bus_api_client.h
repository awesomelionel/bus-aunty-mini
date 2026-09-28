// firmware/include/net/bus_api_client.h
#pragma once
#include <string>

#include "core/arrival_parser.h"

struct FetchResult {
    int httpStatus = 0;
    // Filled only for a 200 whose body was within kMaxArrivalBodyBytes.
    // valid == false on a parse failure or a body that was refused.
    ParsedBusStop parsed;
};

FetchResult fetchBusArrival(const std::string& busStopCode);
