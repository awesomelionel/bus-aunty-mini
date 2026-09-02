#include "arrival_parser.h"

#include <ArduinoJson.h>

#include "iso8601.h"

namespace {

int64_t readEta(JsonVariantConst nextBus) {
    if (nextBus.isNull()) {
        return -1;
    }
    const char* iso = nextBus["EstimatedArrival"] | "";
    if (iso[0] == '\0') {
        return -1;
    }
    return parseIso8601ToEpoch(iso);
}

}  // namespace

ParsedBusStop parseBusArrivalResponse(const std::string& json,
                                       const std::string& expectedStopCode) {
    ParsedBusStop result;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        return result;
    }

    for (JsonObjectConst stop : doc["busStops"].as<JsonArrayConst>()) {
        const char* code = stop["BusStopCode"] | "";
        if (expectedStopCode != code) {
            continue;
        }

        result.busStopCode = code;
        for (JsonObjectConst service : stop["Services"].as<JsonArrayConst>()) {
            BusService svc;
            svc.serviceNo = service["ServiceNo"] | "";
            svc.times.eta1Epoch = readEta(service["NextBus"]);
            svc.times.eta2Epoch = readEta(service["NextBus2"]);
            svc.times.eta3Epoch = readEta(service["NextBus3"]);
            result.services.push_back(svc);
        }
        result.valid = true;
        break;
    }

    return result;
}

std::vector<BusService> selectDisplayServices(
    const std::vector<BusService>& services, size_t maxCount) {
    std::vector<BusService> selected;
    for (size_t i = 0; i < services.size() && i < maxCount; ++i) {
        selected.push_back(services[i]);
    }
    return selected;
}
