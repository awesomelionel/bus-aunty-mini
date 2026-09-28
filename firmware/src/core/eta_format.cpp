#include "core/eta_format.h"

#include "core/arrival_parser.h"

namespace {

// A bus due within a minute, or up to two minutes past its estimate, is
// "Arr". Anything older than that is dropped by pruneExpiredArrivals before
// it is drawn; if one is formatted anyway it is a placeholder, not "Arr".
constexpr int64_t kArrivingWindowSeconds = 60;

}  // namespace

std::string formatEtaMinutes(int64_t targetEpoch, int64_t nowEpoch) {
    if (targetEpoch < 0) {
        return "--";
    }

    int64_t diffSeconds = targetEpoch - nowEpoch;

    if (diffSeconds < -kEtaDropPastSeconds) {
        return "--";
    }
    if (diffSeconds <= kArrivingWindowSeconds) {
        return kEtaArrivingLabel;
    }

    // Floor rather than round to nearest, so 3m30s shows as 3. Understating
    // the wait never leaves you thinking you have more time than you do.
    int64_t minutes = diffSeconds / 60;

    if (minutes > 60) {
        return "60+";
    }
    return std::to_string(minutes);
}
