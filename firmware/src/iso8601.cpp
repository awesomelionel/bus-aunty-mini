#include "iso8601.h"

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
    int fields = sscanf(iso8601.c_str(), "%d-%d-%dT%d:%d:%dZ", &year, &month,
                         &day, &hour, &minute, &second);
    if (fields != 6) {
        return -1;
    }

    int64_t days = daysFromCivil(year, static_cast<unsigned>(month),
                                  static_cast<unsigned>(day));
    return days * 86400 + hour * 3600 + minute * 60 + second;
}
