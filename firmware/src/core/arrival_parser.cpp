#include "core/arrival_parser.h"

#include <ArduinoJson.h>

#include <string>

#include "core/iso8601.h"
#include "core/night_window.h"

#if defined(ESP32) || defined(HOST_STREAM)
#include <Stream.h>
#endif

namespace {

void sortArrivalsByEta(std::vector<BusArrival>& items) {
    for (size_t i = 1; i < items.size(); ++i) {
        BusArrival key = items[i];
        size_t j = i;
        while (j > 0 && items[j - 1].etaEpoch > key.etaEpoch) {
            items[j] = items[j - 1];
            --j;
        }
        items[j] = key;
    }
}

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
    arrival.terminating = nextBus["Terminating"] | false;
    return arrival;
}

std::string readLabel(JsonVariantConst nextBus) {
    if (nextBus.isNull()) {
        return "";
    }
    return nextBus["Label"] | "";
}

std::string stripLeadingZeros(const std::string& s) {
    size_t firstNonZero = s.find_first_not_of('0');
    if (firstNonZero == std::string::npos) {
        return "0";
    }
    return s.substr(firstNonZero);
}

// Keeps the fields the row model reads and drops the rest of a v2 payload
// before it is stored. Terminating stays in the filter so a terminating bus
// is marked rather than discarded with the unused keys.
void buildArrivalFilter(JsonDocument& filter) {
    filter["busStops"][0]["BusStopCode"] = true;
    filter["busStops"][0]["UpdatedAt"] = true;
    filter["busStops"][0]["Services"][0]["ServiceNo"] = true;
    filter["busStops"][0]["Services"][0]["Loop"]["IsLoop"] = true;
    filter["busStops"][0]["Services"][0]["Loop"]["LoopDesc"] = true;
    const char* slots[] = {"NextBus", "NextBus2", "NextBus3"};
    for (const char* slot : slots) {
        filter["busStops"][0]["Services"][0][slot]["EstimatedArrival"] = true;
        filter["busStops"][0]["Services"][0][slot]["Load"] = true;
        filter["busStops"][0]["Services"][0][slot]["Type"] = true;
        filter["busStops"][0]["Services"][0][slot]["VisitNumber"] = true;
        filter["busStops"][0]["Services"][0][slot]["Label"] = true;
        filter["busStops"][0]["Services"][0][slot]["Terminating"] = true;
    }
}

struct RowAcc {
    std::string serviceNo;
    std::string label;
    std::string visit;
    bool isLoop = false;
    // Empty-label rows from different Services[] entries must not merge.
    // A labelled row uses this sentinel so matching labels do merge.
    size_t entryKey = 0;
    size_t firstIndex = 0;
    std::vector<BusArrival> arrivals;
};

constexpr size_t kMergeEntries = static_cast<size_t>(-1);

}  // namespace

JsonDocument& arrivalFilter() {
    static JsonDocument filter;
    static bool built = false;
    if (!built) {
        buildArrivalFilter(filter);
        built = true;
    }
    return filter;
}

ParsedBusStop parsedFromDoc(const JsonDocument& doc,
                            const std::string& expectedStopCode) {
    ParsedBusStop result;

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
        const char* updated = stop["UpdatedAt"] | "";
        if (updated[0] != '\0') {
            result.updatedAtEpoch = parseIso8601ToEpoch(updated);
        }
        for (JsonObjectConst service : stop["Services"].as<JsonArrayConst>()) {
            BusService svc;
            svc.serviceNo = service["ServiceNo"] | "";

            JsonVariantConst loop = service["Loop"];
            if (!loop.isNull()) {
                svc.isLoop = loop["IsLoop"] | false;
                if (!loop["LoopDesc"].isNull()) {
                    svc.loopDesc = loop["LoopDesc"] | "";
                }
            }

            svc.arrivals[0] = readArrival(service["NextBus"]);
            svc.arrivals[1] = readArrival(service["NextBus2"]);
            svc.arrivals[2] = readArrival(service["NextBus3"]);
            svc.labels[0] = readLabel(service["NextBus"]);
            svc.labels[1] = readLabel(service["NextBus2"]);
            svc.labels[2] = readLabel(service["NextBus3"]);
            if (result.services.size() >= kMaxServicesPerStop) {
                break;
            }
            result.services.push_back(svc);
        }
        result.rows = flattenToRows(result.services);
        if (result.rows.size() > kMaxRowsPerStop) {
            result.rows.resize(kMaxRowsPerStop);
        }
        result.valid = true;
        break;
    }

    return result;
}

ParsedBusStop parseBusArrivalResponse(const std::string& json,
                                       const std::string& expectedStopCode) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, json, DeserializationOption::Filter(arrivalFilter()));
    if (err) {
        return ParsedBusStop();
    }
    return parsedFromDoc(doc, expectedStopCode);
}

#if defined(ESP32) || defined(HOST_STREAM)
ParsedBusStop parseBusArrivalStream(Stream& input,
                                    const std::string& expectedStopCode) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, input, DeserializationOption::Filter(arrivalFilter()));
    if (err) {
        return ParsedBusStop();
    }
    return parsedFromDoc(doc, expectedStopCode);
}
#endif

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
    if (label.size() >= 3 && label[0] == 'T' && label[1] == 'o' &&
        label[2] == ' ') {
        return label.substr(3);
    }
    return label;
}

std::vector<BusServiceRow> flattenToRows(const std::vector<BusService>& services) {
    std::vector<RowAcc> acc;
    std::vector<std::string> serviceOrder;
    size_t nextIndex = 0;

    for (size_t entry = 0; entry < services.size(); ++entry) {
        const BusService& svc = services[entry];
        bool seenService = false;
        for (const std::string& name : serviceOrder) {
            if (name == svc.serviceNo) {
                seenService = true;
                break;
            }
        }
        if (!seenService) {
            serviceOrder.push_back(svc.serviceNo);
        }

        std::string slotLabel[kArrivalsPerService];
        std::string slotVisit[kArrivalsPerService];
        for (size_t i = 0; i < kArrivalsPerService; ++i) {
            slotLabel[i] = stripToPrefix(svc.labels[i]);
            slotVisit[i] = svc.arrivals[i].visitNumber;
        }
        // An empty label borrows the first non-empty label in this same
        // entry that shares its VisitNumber. It does not borrow from a
        // different Services[] entry.
        for (size_t i = 0; i < kArrivalsPerService; ++i) {
            if (svc.arrivals[i].etaEpoch < 0 || !slotLabel[i].empty()) {
                continue;
            }
            for (size_t j = 0; j < kArrivalsPerService; ++j) {
                if (!slotLabel[j].empty() && slotVisit[j] == slotVisit[i]) {
                    slotLabel[i] = slotLabel[j];
                    break;
                }
            }
        }
        // Loop description fills a label that is still empty. A null
        // LoopDesc leaves the line blank.
        if (svc.isLoop && !svc.loopDesc.empty()) {
            const std::string fallback = stripToPrefix(svc.loopDesc);
            if (!fallback.empty()) {
                for (size_t i = 0; i < kArrivalsPerService; ++i) {
                    if (svc.arrivals[i].etaEpoch >= 0 && slotLabel[i].empty()) {
                        slotLabel[i] = fallback;
                    }
                }
            }
        }

        for (size_t i = 0; i < kArrivalsPerService; ++i) {
            if (svc.arrivals[i].etaEpoch < 0) {
                continue;
            }
            const bool labelled = !slotLabel[i].empty();
            RowAcc* found = nullptr;
            for (RowAcc& row : acc) {
                if (row.serviceNo != svc.serviceNo || row.label != slotLabel[i] ||
                    row.visit != slotVisit[i]) {
                    continue;
                }
                if (!labelled && row.entryKey != entry) {
                    continue;
                }
                found = &row;
                break;
            }
            if (found == nullptr) {
                RowAcc row;
                row.serviceNo = svc.serviceNo;
                row.label = slotLabel[i];
                row.visit = slotVisit[i];
                row.isLoop = svc.isLoop;
                row.entryKey = labelled ? kMergeEntries : entry;
                row.firstIndex = nextIndex++;
                acc.push_back(row);
                found = &acc.back();
            } else if (svc.isLoop) {
                found->isLoop = true;
            }
            found->arrivals.push_back(svc.arrivals[i]);
        }
    }

    for (size_t entry = 0; entry < services.size(); ++entry) {
        const BusService& svc = services[entry];
        bool already = false;
        for (const RowAcc& row : acc) {
            if (row.serviceNo == svc.serviceNo) {
                already = true;
                break;
            }
        }
        if (!already) {
            RowAcc row;
            row.serviceNo = svc.serviceNo;
            row.isLoop = svc.isLoop;
            row.entryKey = entry;
            row.firstIndex = nextIndex++;
            acc.push_back(row);
        }
    }

    std::vector<BusServiceRow> rows;
    for (const std::string& serviceNo : serviceOrder) {
        std::vector<size_t> idx;
        for (size_t i = 0; i < acc.size(); ++i) {
            if (acc[i].serviceNo == serviceNo) {
                idx.push_back(i);
            }
        }
        // Visit 1 before visit 2, then label, then the order the row was
        // first created. Alphabetical labels stay put when the lead bus
        // changes slot. v2 1.08 keeps first-seen order instead.
        for (size_t i = 1; i < idx.size(); ++i) {
            size_t key = idx[i];
            size_t j = i;
            while (j > 0) {
                const RowAcc& a = acc[key];
                const RowAcc& b = acc[idx[j - 1]];
                const int rankA = a.visit == "2" ? 1 : 0;
                const int rankB = b.visit == "2" ? 1 : 0;
                const bool earlier =
                    rankA < rankB ||
                    (rankA == rankB && a.label < b.label) ||
                    (rankA == rankB && a.label == b.label &&
                     a.firstIndex < b.firstIndex);
                if (!earlier) {
                    break;
                }
                idx[j] = idx[j - 1];
                --j;
            }
            idx[j] = key;
        }

        for (size_t id : idx) {
            RowAcc& data = acc[id];
            sortArrivalsByEta(data.arrivals);
            if (data.arrivals.size() > kArrivalsPerService) {
                data.arrivals.resize(kArrivalsPerService);
            }
            BusServiceRow row;
            row.serviceNo = data.serviceNo;
            row.label = data.label;
            row.visitNumber = data.visit;
            row.isLoop = data.isLoop;
            for (size_t i = 0; i < data.arrivals.size(); ++i) {
                row.arrivals[i] = data.arrivals[i];
            }
            rows.push_back(row);
        }
    }
    return rows;
}

void pruneExpiredArrivals(std::vector<BusServiceRow>& rows, int64_t nowEpoch) {
    if (nowEpoch < kClockSetEpoch) {
        return;
    }
    std::vector<BusServiceRow> kept;
    kept.reserve(rows.size());
    for (BusServiceRow& row : rows) {
        std::array<BusArrival, kArrivalsPerService> next{};
        size_t n = 0;
        for (const BusArrival& arrival : row.arrivals) {
            if (arrival.etaEpoch < 0 || n >= kArrivalsPerService) {
                continue;
            }
            if (nowEpoch - arrival.etaEpoch > kEtaDropPastSeconds) {
                continue;
            }
            next[n++] = arrival;
        }
        if (n == 0) {
            continue;
        }
        row.arrivals = next;
        kept.push_back(row);
    }
    rows.swap(kept);
}

bool rowsHaveArrivals(const std::vector<BusServiceRow>& rows) {
    for (const BusServiceRow& row : rows) {
        for (const BusArrival& arrival : row.arrivals) {
            if (arrival.etaEpoch >= 0) {
                return true;
            }
        }
    }
    return false;
}

bool rowAllTerminating(const BusServiceRow& row) {
    bool any = false;
    for (const BusArrival& arrival : row.arrivals) {
        if (arrival.etaEpoch < 0) {
            continue;
        }
        any = true;
        if (!arrival.terminating) {
            return false;
        }
    }
    return any;
}

size_t servicePageCount(const std::vector<BusServiceRow>& rows, size_t pageSize) {
    if (pageSize == 0 || rows.empty()) {
        return 0;
    }

    size_t pageCount = 0;
    size_t currentPageSize = 0;

    for (size_t i = 0; i < rows.size();) {
        const std::string& serviceNo = rows[i].serviceNo;
        size_t serviceRowCount = 1;
        while (i + serviceRowCount < rows.size() &&
               rows[i + serviceRowCount].serviceNo == serviceNo) {
            ++serviceRowCount;
        }

        if (currentPageSize + serviceRowCount <= pageSize) {
            currentPageSize += serviceRowCount;
            i += serviceRowCount;
        } else if (currentPageSize > 0 && serviceRowCount <= pageSize) {
            ++pageCount;
            currentPageSize = serviceRowCount;
            i += serviceRowCount;
        } else {
            while (currentPageSize < pageSize && i < rows.size()) {
                ++currentPageSize;
                ++i;
            }
            ++pageCount;
            currentPageSize = 0;
        }
    }

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

    std::vector<std::vector<BusServiceRow>> pages;
    std::vector<BusServiceRow> currentPage;

    for (size_t i = 0; i < rows.size();) {
        const std::string& serviceNo = rows[i].serviceNo;
        size_t serviceRowCount = 1;
        while (i + serviceRowCount < rows.size() &&
               rows[i + serviceRowCount].serviceNo == serviceNo) {
            ++serviceRowCount;
        }

        if (currentPage.size() + serviceRowCount <= pageSize) {
            for (size_t j = 0; j < serviceRowCount; ++j) {
                currentPage.push_back(rows[i + j]);
            }
            i += serviceRowCount;
        } else if (!currentPage.empty() && serviceRowCount <= pageSize) {
            pages.push_back(currentPage);
            currentPage.clear();
            for (size_t j = 0; j < serviceRowCount; ++j) {
                currentPage.push_back(rows[i + j]);
            }
            i += serviceRowCount;
        } else {
            while (currentPage.size() < pageSize && i < rows.size()) {
                currentPage.push_back(rows[i++]);
            }
            if (!currentPage.empty()) {
                pages.push_back(currentPage);
                currentPage.clear();
            }
        }
    }

    if (!currentPage.empty()) {
        pages.push_back(currentPage);
    }
    if (page < pages.size()) {
        return pages[page];
    }
    return selected;
}

bool shouldShowVisit2Marker(const BusServiceRow& row) {
    if (row.label.empty() || rowAllTerminating(row)) {
        return false;
    }
    bool hasArrival = false;
    for (const BusArrival& arrival : row.arrivals) {
        if (arrival.etaEpoch < 0) {
            continue;
        }
        hasArrival = true;
        if (arrival.visitNumber != "2") {
            return false;
        }
    }
    return hasArrival;
}
