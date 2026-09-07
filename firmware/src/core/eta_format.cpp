#include "core/eta_format.h"

namespace {

// The upstream ETA is an estimate, and a bus counted as "arrived" may not
// have reached this stop yet, so anything within this much of the estimate --
// or already past it -- counts as arriving.
constexpr int64_t kArrivingWindowSeconds = 120;

}  // namespace

std::string formatEtaMinutes(int64_t targetEpoch, int64_t nowEpoch) {
    if (targetEpoch < 0) {
        return "--";
    }

    int64_t diffSeconds = targetEpoch - nowEpoch;

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
