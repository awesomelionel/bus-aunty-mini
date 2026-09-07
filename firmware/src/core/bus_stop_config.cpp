#include "core/bus_stop_config.h"

namespace {

// Field and record separators for the persisted blob. These are control
// characters, which normalizeBusStopName strips, so a name can never contain
// one and break the encoding.
constexpr char kFieldSep = '\x1f';
constexpr char kRecordSep = '\x1e';

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v';
}

std::string trim(const std::string& raw) {
    size_t begin = 0;
    while (begin < raw.size() && isSpace(raw[begin])) {
        ++begin;
    }
    size_t end = raw.size();
    while (end > begin && isSpace(raw[end - 1])) {
        --end;
    }
    return raw.substr(begin, end - begin);
}

}  // namespace

const std::string& busStopLabel(const BusStopConfig& stop) {
    return stop.name.empty() ? stop.code : stop.name;
}

bool normalizeBusStopCode(const std::string& raw, std::string* out) {
    std::string trimmed = trim(raw);
    if (trimmed.size() < kBusStopCodeMinDigits ||
        trimmed.size() > kBusStopCodeMaxDigits) {
        return false;
    }
    for (char c : trimmed) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    if (out != nullptr) {
        *out = trimmed;
    }
    return true;
}

std::string normalizeBusStopName(const std::string& raw) {
    std::string cleaned;
    for (char c : raw) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 0x20 || uc == 0x7f) {
            continue;  // control characters would break the persisted blob
        }
        // The portal renders values into value='...' unescaped, so these would
        // truncate the field the next time the form is opened.
        if (c == '\'' || c == '<' || c == '>') {
            continue;
        }
        cleaned.push_back(c);
    }
    cleaned = trim(cleaned);
    if (cleaned.size() > kBusStopNameMaxChars) {
        cleaned.resize(kBusStopNameMaxChars);
        cleaned = trim(cleaned);
    }
    return cleaned;
}

std::vector<BusStopConfig> buildBusStopList(
    const std::vector<BusStopConfig>& rows) {
    std::vector<BusStopConfig> stops;
    for (const BusStopConfig& row : rows) {
        if (stops.size() >= kMaxBusStops) {
            break;
        }
        BusStopConfig stop;
        if (!normalizeBusStopCode(row.code, &stop.code)) {
            continue;
        }
        stop.name = normalizeBusStopName(row.name);
        stops.push_back(stop);
    }
    return stops;
}

std::string serializeBusStops(const std::vector<BusStopConfig>& stops) {
    std::string blob;
    for (size_t i = 0; i < stops.size(); ++i) {
        if (i > 0) {
            blob.push_back(kRecordSep);
        }
        blob += stops[i].code;
        blob.push_back(kFieldSep);
        blob += stops[i].name;
    }
    return blob;
}

std::vector<BusStopConfig> deserializeBusStops(const std::string& blob) {
    std::vector<BusStopConfig> rows;
    size_t start = 0;
    while (start <= blob.size()) {
        size_t recordEnd = blob.find(kRecordSep, start);
        if (recordEnd == std::string::npos) {
            recordEnd = blob.size();
        }
        std::string record = blob.substr(start, recordEnd - start);
        size_t fieldEnd = record.find(kFieldSep);
        BusStopConfig row;
        if (fieldEnd == std::string::npos) {
            row.code = record;
        } else {
            row.code = record.substr(0, fieldEnd);
            row.name = record.substr(fieldEnd + 1);
        }
        rows.push_back(row);
        if (recordEnd == blob.size()) {
            break;
        }
        start = recordEnd + 1;
    }
    // Re-validate, so a corrupt or truncated blob cannot yield bad stops.
    return buildBusStopList(rows);
}
