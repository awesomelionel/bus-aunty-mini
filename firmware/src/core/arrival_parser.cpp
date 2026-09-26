#include "core/arrival_parser.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <map>
#include <set>
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
    
    // Key rows by (serviceNo, label) only - merge arrivals from both visit numbers
    struct RowKey {
        std::string serviceNo;
        std::string label;
        bool isLoop;
        
        bool operator<(const RowKey& other) const {
            if (serviceNo != other.serviceNo) return serviceNo < other.serviceNo;
            return label < other.label;
        }
        bool operator==(const RowKey& other) const {
            return serviceNo == other.serviceNo && label == other.label;
        }
    };
    
    struct RowData {
        std::vector<BusArrival> arrivals;
        bool isLoop = false;
        int firstSeenVisit = 0;  // Track whether visit "1" or "2" was seen first
        size_t firstSeenIndex = 0;  // Track order within service
    };
    
    std::map<RowKey, RowData> rowData;
    std::vector<std::string> serviceOrder;  // Track service order from API
    std::map<std::string, size_t> serviceRowIndex;  // Track row index within service
    std::set<std::string> servicesWithArrivals;  // Track which services have at least one arrival
    
    for (const BusService& svc : services) {
        // Track first-seen service order
        if (std::find(serviceOrder.begin(), serviceOrder.end(), svc.serviceNo) == serviceOrder.end()) {
            serviceOrder.push_back(svc.serviceNo);
            serviceRowIndex[svc.serviceNo] = 0;
        }
        
        for (size_t i = 0; i < kArrivalsPerService; ++i) {
            const BusArrival& arr = svc.arrivals[i];
            if (arr.etaEpoch < 0) {
                continue;  // Skip empty slots
            }
            servicesWithArrivals.insert(svc.serviceNo);
            
            std::string label = stripToPrefix(svc.labels[i]);
            RowKey key{svc.serviceNo, label, svc.isLoop};
            
            if (rowData.find(key) == rowData.end()) {
                // First time seeing this (service, label) pair
                rowData[key].isLoop = svc.isLoop;
                rowData[key].firstSeenIndex = serviceRowIndex[svc.serviceNo]++;
                // Track whether visit "1" or "2" appeared first
                rowData[key].firstSeenVisit = (arr.visitNumber == "2") ? 2 : 1;
            }
            rowData[key].arrivals.push_back(arr);
        }
    }
    
    // Create empty rows for services with no arrivals across all entries
    for (const std::string& serviceNo : serviceOrder) {
        if (servicesWithArrivals.find(serviceNo) == servicesWithArrivals.end()) {
            RowKey key{serviceNo, "", false};  // isLoop doesn't matter for empty rows
            if (rowData.find(key) == rowData.end()) {
                rowData[key].isLoop = false;
                rowData[key].firstSeenIndex = serviceRowIndex[serviceNo]++;
                rowData[key].firstSeenVisit = 1;
                // arrivals vector is left empty
            }
        }
    }
    
    // Build rows: services in API order, within each service visit-1 rows before visit-2 rows,
    // preserving first-seen order within each visit number
    for (const std::string& serviceNo : serviceOrder) {
        std::vector<std::pair<RowKey, RowData*>> serviceRows;
        for (auto& pair : rowData) {
            if (pair.first.serviceNo == serviceNo) {
                serviceRows.push_back({pair.first, &pair.second});
            }
        }
        
        // Sort: visit-1 rows (firstSeenVisit==1) before visit-2 rows (firstSeenVisit==2),
        // then by firstSeenIndex to preserve API order
        std::sort(serviceRows.begin(), serviceRows.end(),
                  [](const std::pair<RowKey, RowData*>& a, const std::pair<RowKey, RowData*>& b) {
                      if (a.second->firstSeenVisit != b.second->firstSeenVisit) {
                          return a.second->firstSeenVisit < b.second->firstSeenVisit;
                      }
                      return a.second->firstSeenIndex < b.second->firstSeenIndex;
                  });
        
        for (const auto& pair : serviceRows) {
            const RowKey& key = pair.first;
            RowData* data = pair.second;
            
            BusServiceRow row;
            row.serviceNo = key.serviceNo;
            row.label = key.label;
            row.isLoop = data->isLoop;
            
            // Sort arrivals by ETA
            std::sort(data->arrivals.begin(), data->arrivals.end(),
                      [](const BusArrival& a, const BusArrival& b) {
                          return a.etaEpoch < b.etaEpoch;
                      });
            
            // Copy up to kArrivalsPerService arrivals
            for (size_t i = 0; i < data->arrivals.size() && i < kArrivalsPerService; ++i) {
                row.arrivals[i] = data->arrivals[i];
            }
            // Fill remaining slots with empty arrivals
            for (size_t i = data->arrivals.size(); i < kArrivalsPerService; ++i) {
                row.arrivals[i] = BusArrival{};
            }
            
            rows.push_back(row);
        }
    }
    
    // For each service, find distinct non-empty labels and collect all arrivals
    std::map<std::string, std::set<std::string>> serviceLabels;
    std::map<std::string, std::vector<BusArrival>> serviceEmptyLabelArrivals;
    
    for (const BusServiceRow& row : rows) {
        if (!row.label.empty()) {
            serviceLabels[row.serviceNo].insert(row.label);
        } else {
            // Collect arrivals from empty-label rows
            for (const BusArrival& arr : row.arrivals) {
                if (arr.etaEpoch >= 0) {
                    serviceEmptyLabelArrivals[row.serviceNo].push_back(arr);
                }
            }
        }
    }
    
    // Identify services where empty-label arrivals should be merged
    std::set<std::string> servicesToMerge;
    for (const auto& pair : serviceLabels) {
        if (pair.second.size() == 1 && serviceEmptyLabelArrivals.count(pair.first)) {
            servicesToMerge.insert(pair.first);
        }
    }
    
    // Build final row list, merging empty-label arrivals where needed
    std::vector<BusServiceRow> mergedRows;
    for (BusServiceRow& row : rows) {
        const std::string& svc = row.serviceNo;
        
        // Skip empty-label rows that will be merged
        if (row.label.empty() && servicesToMerge.count(svc)) {
            continue;
        }
        
        // If this is the labeled row for a service that needs merging, merge now
        if (!row.label.empty() && servicesToMerge.count(svc)) {
            // Collect all arrivals (existing + empty-label ones)
            std::vector<BusArrival> allArrivals;
            for (const BusArrival& arr : row.arrivals) {
                if (arr.etaEpoch >= 0) {
                    allArrivals.push_back(arr);
                }
            }
            for (const BusArrival& arr : serviceEmptyLabelArrivals[svc]) {
                allArrivals.push_back(arr);
            }
            // Sort by ETA
            std::sort(allArrivals.begin(), allArrivals.end(),
                      [](const BusArrival& a, const BusArrival& b) {
                          return a.etaEpoch < b.etaEpoch;
                      });
            // Copy back
            for (size_t i = 0; i < kArrivalsPerService; ++i) {
                if (i < allArrivals.size()) {
                    row.arrivals[i] = allArrivals[i];
                } else {
                    row.arrivals[i] = BusArrival{};
                }
            }
        }
        
        mergedRows.push_back(row);
    }
    rows = mergedRows;
    
    // Determine which services should show labels
    // A service shows labels if: (1) it has 2+ distinct non-empty labels, OR (2) it's a loop
    std::map<std::string, std::set<std::string>> serviceNonEmptyLabels;
    std::map<std::string, bool> serviceIsLoop;
    for (const BusServiceRow& row : rows) {
        if (!row.label.empty()) {
            serviceNonEmptyLabels[row.serviceNo].insert(row.label);
        }
        if (row.isLoop) {
            serviceIsLoop[row.serviceNo] = true;
        }
    }
    
    // Clear labels for services that don't meet the criteria
    for (BusServiceRow& row : rows) {
        bool showLabel = serviceIsLoop[row.serviceNo] || 
                        serviceNonEmptyLabels[row.serviceNo].size() >= 2;
        if (!showLabel) {
            row.label.clear();
        }
    }
    
    return rows;
}

size_t servicePageCount(size_t serviceCount, size_t pageSize) {
    if (pageSize == 0 || serviceCount == 0) {
        return 0;
    }
    // Simple calculation - actual pages may differ due to service grouping
    // but this is used for initial estimates
    return (serviceCount + pageSize - 1) / pageSize;
}

size_t servicePageCount(const std::vector<BusServiceRow>& rows, size_t pageSize) {
    if (pageSize == 0 || rows.empty()) {
        return 0;
    }
    
    // Simulate paging with service grouping to get accurate page count
    size_t pageCount = 0;
    size_t currentPageSize = 0;
    
    for (size_t i = 0; i < rows.size(); ) {
        const std::string& serviceNo = rows[i].serviceNo;
        
        // Count consecutive rows with same serviceNo
        size_t serviceRowCount = 1;
        while (i + serviceRowCount < rows.size() && 
               rows[i + serviceRowCount].serviceNo == serviceNo) {
            ++serviceRowCount;
        }
        
        // If service rows fit on current page, add them
        if (currentPageSize + serviceRowCount <= pageSize) {
            currentPageSize += serviceRowCount;
            i += serviceRowCount;
        }
        // If service rows would straddle boundary but fit on next page
        else if (currentPageSize > 0 && serviceRowCount <= pageSize) {
            // Start new page with this service
            ++pageCount;
            currentPageSize = serviceRowCount;
            i += serviceRowCount;
        }
        // Service is too large for one page, split it
        else {
            // Fill current page as much as possible
            while (currentPageSize < pageSize && i < rows.size()) {
                ++currentPageSize;
                ++i;
            }
            ++pageCount;
            currentPageSize = 0;
        }
    }
    
    // Add final page if non-empty
    if (currentPageSize > 0) {
        ++pageCount;
    }
    
    return pageCount;
}

std::vector<BusServiceRow> selectServicePage(
    const std::vector<BusServiceRow>& rows, size_t pageSize, size_t page) {
    std::vector<BusServiceRow> selected;
    
    if (pageSize == 0 || rows.empty()) {
        return selected;
    }
    
    // Build pages respecting service boundaries
    std::vector<std::vector<BusServiceRow>> pages;
    std::vector<BusServiceRow> currentPage;
    
    for (size_t i = 0; i < rows.size(); ) {
        const std::string& serviceNo = rows[i].serviceNo;
        
        // Count consecutive rows with same serviceNo
        size_t serviceRowCount = 1;
        while (i + serviceRowCount < rows.size() && 
               rows[i + serviceRowCount].serviceNo == serviceNo) {
            ++serviceRowCount;
        }
        
        // If service rows fit on current page, add them
        if (currentPage.size() + serviceRowCount <= pageSize) {
            for (size_t j = 0; j < serviceRowCount; ++j) {
                currentPage.push_back(rows[i + j]);
            }
            i += serviceRowCount;
        }
        // If service rows would straddle boundary but fit on next page
        else if (!currentPage.empty() && serviceRowCount <= pageSize) {
            // Push current page and start new one with this service
            pages.push_back(currentPage);
            currentPage.clear();
            for (size_t j = 0; j < serviceRowCount; ++j) {
                currentPage.push_back(rows[i + j]);
            }
            i += serviceRowCount;
        }
        // Service is too large for one page, split it
        else {
            // Fill current page as much as possible
            while (currentPage.size() < pageSize && i < rows.size()) {
                currentPage.push_back(rows[i++]);
            }
            if (!currentPage.empty()) {
                pages.push_back(currentPage);
                currentPage.clear();
            }
        }
    }
    
    // Add final page if non-empty
    if (!currentPage.empty()) {
        pages.push_back(currentPage);
    }
    
    // Return requested page
    if (page < pages.size()) {
        return pages[page];
    }
    return selected;  // Empty if page out of range
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
