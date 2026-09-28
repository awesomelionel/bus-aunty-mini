#include "core/iso8601.h"

#include <cstdio>

namespace {

// Days since 1970-01-01 for a proleptic-Gregorian civil date. Pure integer
// arithmetic (no libc time functions) so it behaves identically on the
// ESP32 Arduino toolchain and in native host unit tests. Algorithm from
// https://howardhinnant.github.io/date_algorithms.html#days_from_civil
int64_t daysFromCivil(int64_t year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int64_t era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy =
        (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

}  // namespace

int64_t parseIso8601ToEpoch(const std::string& iso8601) {
    int year, month, day, hour, minute, second;
    int consumed = 0;
    int fields = sscanf(iso8601.c_str(), "%d-%d-%dT%d:%d:%d%n", &year, &month,
                         &day, &hour, &minute, &second, &consumed);
    if (fields != 6) {
        return -1;
    }
    if (month < 1 || month > 12 || hour < 0 || hour > 23 || minute < 0 ||
        minute > 59 || second < 0 || second > 59) {
        return -1;
    }
    const int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int dim = daysInMonth[month];
    const bool leap =
        (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (month == 2 && leap) {
        dim = 29;
    }
    if (day < 1 || day > dim) {
        return -1;
    }

    int64_t days = daysFromCivil(year, static_cast<unsigned>(month),
                                  static_cast<unsigned>(day));
    int64_t epoch = days * 86400 + hour * 3600 + minute * 60 + second;

    const char* tail = iso8601.c_str() + consumed;
    // UpdatedAt carries fractional seconds ("...03.630381+08:00"). The epoch
    // is whole seconds, so the fraction is skipped rather than rounded.
    if (tail[0] == '.') {
        ++tail;
        while (tail[0] >= '0' && tail[0] <= '9') {
            ++tail;
        }
    }
    // A trailing Z is UTC. A timestamp with no offset is not assumed to be UTC.
    if (tail[0] == 'Z' && tail[1] == '\0') {
        return epoch;
    }
    if (tail[0] == '\0') {
        return -1;
    }
    if (tail[0] == '+' || tail[0] == '-') {
        int offsetHour, offsetMinute;
        if (sscanf(tail + 1, "%2d:%2d", &offsetHour, &offsetMinute) == 2) {
            int64_t offsetSeconds = offsetHour * 3600 + offsetMinute * 60;
            return (tail[0] == '+') ? epoch - offsetSeconds : epoch + offsetSeconds;
        }
    }
    return -1;
}
