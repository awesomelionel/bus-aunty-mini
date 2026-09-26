#include "core/arrival_parser.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <map>
#include <string>

#include "core/iso8601.h"

namespace {

BusArrival readArrival(JsonVariantConst nextBus) {
    BusArrival arrival;
    if (nextBus.isNull()) {
        return arrival;
    }
    const char* iso = nextBus["EstimatedArrival"] | "";
    if (iso[0] == '\0') {
        return arrival;
    }
    arrival.etaEpoch = parseIso8601ToEpoch(iso);
    arrival.load = parseBusLoad(nextBus["Load"] | "");
    arrival.type = parseBusType(nextBus["Type"] | "");
    arrival.visitNumber = nextBus["VisitNumber"] | "";
    return arrival;
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
            
            JsonVariantConst loop = service["Loop"];
            if (!loop.isNull()) {
                svc.isLoop = loop["IsLoop"] | false;
            }
            
            svc.arrivals[0] = readArrival(service["NextBus"]);
            svc.arrivals[1] = readArrival(service["NextBus2"]);
            svc.arrivals[2] = readArrival(service["NextBus3"]);
            
            // Extract labels from arrivals (v2 API)
            JsonVariantConst nb = service["NextBus"];
            if (!nb.isNull()) {
                svc.labels[0] = nb["Label"] | "";
            }
            nb = service["NextBus2"];
            if (!nb.isNull()) {
                svc.labels[1] = nb["Label"] | "";
            }
            nb = service["NextBus3"];
            if (!nb.isNull()) {
                svc.labels[2] = nb["Label"] | "";
            }
            
            result.services.push_back(svc);
        }
        result.rows = flattenToRows(result.services);
        result.valid = true;
        break;
    }

    return result;
}

BusLoad parseBusLoad(const std::string& raw) {
    if (raw == "SEA") {
        return BusLoad::SeatsAvailable;
    }
    if (raw == "SDA") {
        return BusLoad::StandingAvailable;
    }
    if (raw == "LSD") {
        return BusLoad::LimitedStanding;
    }
    return BusLoad::Unknown;
}

BusType parseBusType(const std::string& raw) {
    if (raw == "SD") {
        return BusType::SingleDeck;
    }
    if (raw == "DD") {
        return BusType::DoubleDeck;
    }
    if (raw == "BD") {
        return BusType::Bendy;
    }
    return BusType::Unknown;
}

std::string stripToPrefix(const std::string& label) {
    if (label.empty()) {
        return "";
    }
    if (label.size() >= 3 && label[0] == 'T' && label[1] == 'o' && label[2] == ' ') {
        return label.substr(3);
    }
    return label;
}

std::vector<BusServiceRow> flattenToRows(const std::vector<BusService>& services) {
    std::vector<BusServiceRow> rows;
    
    // First, collect all unique (serviceNo, label) pairs with their arrivals
    struct RowKey {
        std::string serviceNo;
        std::string label;
        bool isLoop;
        int sortOrder;  // For loop services: VisitNumber "1" before "2"
        
        bool operator<(const RowKey& other) const {
            if (serviceNo != other.serviceNo) return serviceNo < other.serviceNo;
            if (sortOrder != other.sortOrder) return sortOrder < other.sortOrder;
            return label < other.label;
        }
        bool operator==(const RowKey& other) const {
            return serviceNo == other.serviceNo && label == other.label;
        }
    };
    
    std::map<RowKey, std::vector<BusArrival>> rowArrivals;
    std::vector<RowKey> rowOrder;  // Preserve API order
    
    for (const BusService& svc : services) {
        for (size_t i = 0; i < kArrivalsPerService; ++i) {
            const BusArrival& arr = svc.arrivals[i];
            if (arr.etaEpoch < 0) {
                continue;  // Skip empty slots
            }
            
            std::string label = stripToPrefix(svc.labels[i]);
            
            // Determine sort order for loops: VisitNumber "1" before "2"
            int sortOrder = 0;
            if (svc.isLoop && arr.visitNumber == "2") {
                sortOrder = 1;
            }
            
            RowKey key{svc.serviceNo, label, svc.isLoop, sortOrder};
            
            if (rowArrivals.find(key) == rowArrivals.end()) {
                rowOrder.push_back(key);
            }
            rowArrivals[key].push_back(arr);
        }
    }
    
    // Now create rows in API order, sorting arrivals by ETA within each row
    for (const RowKey& key : rowOrder) {
        BusServiceRow row;
        row.serviceNo = key.serviceNo;
        row.label = key.label;
        row.isLoop = key.isLoop;
        
        // Sort arrivals by ETA
        std::vector<BusArrival>& arrs = rowArrivals[key];
        std::sort(arrs.begin(), arrs.end(),
                  [](const BusArrival& a, const BusArrival& b) {
                      return a.etaEpoch < b.etaEpoch;
                  });
        
        // Copy up to kArrivalsPerService arrivals
        for (size_t i = 0; i < arrs.size() && i < kArrivalsPerService; ++i) {
            row.arrivals[i] = arrs[i];
        }
        // Fill remaining slots with empty arrivals
        for (size_t i = arrs.size(); i < kArrivalsPerService; ++i) {
            row.arrivals[i] = BusArrival{};
        }
        
        rows.push_back(row);
    }
    
    // Determine which services have multiple directions at this stop
    // A service needs a label if: (1) it appears in multiple rows, OR (2) it's a loop
    std::map<std::string, int> serviceRowCounts;
    for (const BusServiceRow& row : rows) {
        serviceRowCounts[row.serviceNo]++;
    }
    
    // Clear labels for single-direction non-loop services
    for (BusServiceRow& row : rows) {
        if (!row.isLoop && serviceRowCounts[row.serviceNo] == 1) {
            row.label.clear();
        }
    }
    
    return rows;
}

size_t servicePageCount(size_t serviceCount, size_t pageSize) {
    if (pageSize == 0) {
        return 0;
    }
    return (serviceCount + pageSize - 1) / pageSize;
}

std::vector<BusServiceRow> selectServicePage(
    const std::vector<BusServiceRow>& rows, size_t pageSize, size_t page) {
    std::vector<BusServiceRow> selected;
    size_t start = page * pageSize;
    for (size_t i = start; i < rows.size() && i < start + pageSize; ++i) {
        selected.push_back(rows[i]);
    }
    return selected;
}

std::vector<BusService> selectServicePage(
    const std::vector<BusService>& services, size_t pageSize, size_t page) {
    std::vector<BusService> selected;
    size_t start = page * pageSize;
    for (size_t i = start; i < services.size() && i < start + pageSize; ++i) {
        selected.push_back(services[i]);
    }
    return selected;
}
