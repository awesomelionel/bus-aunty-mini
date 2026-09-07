#include "core/eta_format.h"

std::string formatEtaMinutes(int64_t targetEpoch, int64_t nowEpoch) {
    if (targetEpoch < 0) {
        return "--";
    }

    // Floor rather than round to nearest, so 2m30s shows as 2. Understating
    // the wait never leaves you thinking you have more time than you do.
    int64_t minutes = (targetEpoch - nowEpoch) / 60;

    if (minutes <= 0) {
        return kEtaArrivingLabel;
    }
    if (minutes > 60) {
        return "60+";
    }
    return std::to_string(minutes);
}
