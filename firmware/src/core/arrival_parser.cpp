#include "core/arrival_parser.h"

#include <ArduinoJson.h>

#include <algorithm>
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
    struct RowData {
        std::string serviceNo;
        std::string label;
        bool isLoop = false;
        std::vector<BusArrival> arrivals;
        int firstSeenVisit = 0;
        size_t firstSeenIndex = 0;
    };
    
    std::vector<RowData> rowData;
    std::vector<std::string> serviceOrder;
    std::vector<std::string> servicesWithArrivals;
    
    for (const BusService& svc : services) {
        // Track first-seen service order
        bool foundService = false;
        for (const std::string& s : serviceOrder) {
            if (s == svc.serviceNo) {
                foundService = true;
                break;
            }
        }
        if (!foundService) {
            serviceOrder.push_back(svc.serviceNo);
        }
        
        for (size_t i = 0; i < kArrivalsPerService; ++i) {
            const BusArrival& arr = svc.arrivals[i];
            if (arr.etaEpoch < 0) {
                continue;
            }
            
            // Track services with arrivals
            bool foundWithArrivals = false;
            for (const std::string& s : servicesWithArrivals) {
                if (s == svc.serviceNo) {
                    foundWithArrivals = true;
                    break;
                }
            }
            if (!foundWithArrivals) {
                servicesWithArrivals.push_back(svc.serviceNo);
            }
            
            std::string label = stripToPrefix(svc.labels[i]);
            
            // Find or create row
            RowData* found = nullptr;
            for (RowData& rd : rowData) {
                if (rd.serviceNo == svc.serviceNo && rd.label == label) {
                    found = &rd;
                    break;
                }
            }
            
            if (!found) {
                RowData newRow;
                newRow.serviceNo = svc.serviceNo;
                newRow.label = label;
                newRow.isLoop = svc.isLoop;
                newRow.firstSeenVisit = (arr.visitNumber == "2") ? 2 : 1;
                // Count existing rows for this service
                size_t idx = 0;
                for (const RowData& rd : rowData) {
                    if (rd.serviceNo == svc.serviceNo) ++idx;
                }
                newRow.firstSeenIndex = idx;
                rowData.push_back(newRow);
                found = &rowData.back();
            }
            found->arrivals.push_back(arr);
        }
    }
    
    // Create empty rows for services with no arrivals
    for (const std::string& serviceNo : serviceOrder) {
        bool hasArrivals = false;
        for (const std::string& s : servicesWithArrivals) {
            if (s == serviceNo) {
                hasArrivals = true;
                break;
            }
        }
        if (!hasArrivals) {
            bool alreadyHasRow = false;
            for (const RowData& rd : rowData) {
                if (rd.serviceNo == serviceNo) {
                    alreadyHasRow = true;
                    break;
                }
            }
            if (!alreadyHasRow) {
                RowData newRow;
                newRow.serviceNo = serviceNo;
                newRow.label = "";
                newRow.isLoop = false;
                newRow.firstSeenVisit = 1;
                newRow.firstSeenIndex = 0;
                rowData.push_back(newRow);
            }
        }
    }
    
    // Build output rows
    std::vector<BusServiceRow> rows;
    for (const std::string& serviceNo : serviceOrder) {
        // Collect rows for this service
        std::vector<RowData*> serviceRows;
        for (RowData& rd : rowData) {
            if (rd.serviceNo == serviceNo) {
                serviceRows.push_back(&rd);
            }
        }
        
        // Sort by visit then index
        for (size_t i = 0; i < serviceRows.size(); ++i) {
            for (size_t j = i + 1; j < serviceRows.size(); ++j) {
                bool swap = false;
                if (serviceRows[i]->firstSeenVisit > serviceRows[j]->firstSeenVisit) {
                    swap = true;
                } else if (serviceRows[i]->firstSeenVisit == serviceRows[j]->firstSeenVisit &&
                          serviceRows[i]->firstSeenIndex > serviceRows[j]->firstSeenIndex) {
                    swap = true;
                }
                if (swap) {
                    RowData* tmp = serviceRows[i];
                    serviceRows[i] = serviceRows[j];
                    serviceRows[j] = tmp;
                }
            }
        }
        
        for (RowData* data : serviceRows) {
            // Sort arrivals by ETA
            for (size_t i = 0; i < data->arrivals.size(); ++i) {
                for (size_t j = i + 1; j < data->arrivals.size(); ++j) {
                    if (data->arrivals[i].etaEpoch > data->arrivals[j].etaEpoch) {
                        BusArrival tmp = data->arrivals[i];
                        data->arrivals[i] = data->arrivals[j];
                        data->arrivals[j] = tmp;
                    }
                }
            }
            
            BusServiceRow row;
            row.serviceNo = data->serviceNo;
            row.label = data->label;
            row.isLoop = data->isLoop;
            
            for (size_t i = 0; i < kArrivalsPerService && i < data->arrivals.size(); ++i) {
                row.arrivals[i] = data->arrivals[i];
            }
            for (size_t i = data->arrivals.size(); i < kArrivalsPerService; ++i) {
                row.arrivals[i] = BusArrival{};
            }
            
            rows.push_back(row);
        }
    }
    
    // For each service, find distinct non-empty labels and collect empty-label arrivals
    struct ServiceLabelInfo {
        std::string serviceNo;
        std::vector<std::string> distinctLabels;
        std::vector<BusArrival> emptyLabelArrivals;
    };
    std::vector<ServiceLabelInfo> labelInfo;
    
    for (const BusServiceRow& row : rows) {
        ServiceLabelInfo* info = nullptr;
        for (ServiceLabelInfo& si : labelInfo) {
            if (si.serviceNo == row.serviceNo) {
                info = &si;
                break;
            }
        }
        if (!info) {
            ServiceLabelInfo newInfo;
            newInfo.serviceNo = row.serviceNo;
            labelInfo.push_back(newInfo);
            info = &labelInfo.back();
        }
        
        if (!row.label.empty()) {
            bool found = false;
            for (const std::string& lbl : info->distinctLabels) {
                if (lbl == row.label) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                info->distinctLabels.push_back(row.label);
            }
        } else {
            for (const BusArrival& arr : row.arrivals) {
                if (arr.etaEpoch >= 0) {
                    info->emptyLabelArrivals.push_back(arr);
                }
            }
        }
    }
    
    // Identify services to merge (1 distinct label + has empty-label arrivals)
    std::vector<std::string> servicesToMerge;
    for (const ServiceLabelInfo& si : labelInfo) {
        if (si.distinctLabels.size() == 1 && !si.emptyLabelArrivals.empty()) {
            servicesToMerge.push_back(si.serviceNo);
        }
    }
    
    // Build final row list with merging
    std::vector<BusServiceRow> mergedRows;
    for (BusServiceRow& row : rows) {
        bool shouldMerge = false;
        for (const std::string& svc : servicesToMerge) {
            if (row.serviceNo == svc) {
                shouldMerge = true;
                break;
            }
        }
        
        // Skip empty-label rows that will be merged
        if (row.label.empty() && shouldMerge) {
            continue;
        }
        
        // Merge empty-label arrivals into labeled row
        if (!row.label.empty() && shouldMerge) {
            std::vector<BusArrival> allArrivals;
            for (const BusArrival& arr : row.arrivals) {
                if (arr.etaEpoch >= 0) {
                    allArrivals.push_back(arr);
                }
            }
            
            // Find empty-label arrivals for this service
            for (const ServiceLabelInfo& si : labelInfo) {
                if (si.serviceNo == row.serviceNo) {
                    for (const BusArrival& arr : si.emptyLabelArrivals) {
                        allArrivals.push_back(arr);
                    }
                    break;
                }
            }
            
            // Sort by ETA using bubble sort
            for (size_t i = 0; i < allArrivals.size(); ++i) {
                for (size_t j = i + 1; j < allArrivals.size(); ++j) {
                    if (allArrivals[i].etaEpoch > allArrivals[j].etaEpoch) {
                        BusArrival tmp = allArrivals[i];
                        allArrivals[i] = allArrivals[j];
                        allArrivals[j] = tmp;
                    }
                }
            }
            
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
    struct ServiceDisplayInfo {
        std::string serviceNo;
        std::vector<std::string> nonEmptyLabels;
        bool isLoop = false;
    };
    std::vector<ServiceDisplayInfo> displayInfo;
    
    for (const BusServiceRow& row : rows) {
        ServiceDisplayInfo* info = nullptr;
        for (ServiceDisplayInfo& di : displayInfo) {
            if (di.serviceNo == row.serviceNo) {
                info = &di;
                break;
            }
        }
        if (!info) {
            ServiceDisplayInfo newInfo;
            newInfo.serviceNo = row.serviceNo;
            newInfo.isLoop = false;
            displayInfo.push_back(newInfo);
            info = &displayInfo.back();
        }
        
        if (!row.label.empty()) {
            bool found = false;
            for (const std::string& lbl : info->nonEmptyLabels) {
                if (lbl == row.label) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                info->nonEmptyLabels.push_back(row.label);
            }
        }
        if (row.isLoop) {
            info->isLoop = true;
        }
    }
    
    // Clear labels for services that don't show labels
    for (BusServiceRow& row : rows) {
        bool showLabel = false;
        for (const ServiceDisplayInfo& di : displayInfo) {
            if (di.serviceNo == row.serviceNo) {
                showLabel = di.isLoop || di.nonEmptyLabels.size() >= 2;
                break;
            }
        }
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

bool shouldShowVisit2Marker(const BusServiceRow& row) {
    bool hasNonEmptyArrival = false;
    for (size_t i = 0; i < kArrivalsPerService; ++i) {
        if (row.arrivals[i].etaEpoch >= 0) {
            hasNonEmptyArrival = true;
            if (row.arrivals[i].visitNumber != "2") {
                return false;  // Found non-visit-2, don't show marker
            }
        }
    }
    return hasNonEmptyArrival;  // Show marker only if has arrivals and all are visit 2
}
