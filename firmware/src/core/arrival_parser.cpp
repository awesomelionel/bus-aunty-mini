#include "arrival_parser.h"

#include <ArduinoJson.h>

#include <string>

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

std::string stripLeadingZeros(const std::string& s) {
    size_t firstNonZero = s.find_first_not_of('0');
    if (firstNonZero == std::string::npos) {
        return "0";  // all zeros (or empty) -> canonical "0"
    }
    return s.substr(firstNonZero);
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
        JsonVariantConst codeVar = stop["BusStopCode"];
        bool matched = false;
        if (codeVar.is<const char*>()) {
            matched = (expectedStopCode == codeVar.as<const char*>());
        } else if (codeVar.is<long long>()) {
            std::string numericCode = std::to_string(codeVar.as<long long>());
            matched = (stripLeadingZeros(expectedStopCode) == numericCode);
        }
        if (!matched) {
            continue;
        }

        result.busStopCode = expectedStopCode;
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
