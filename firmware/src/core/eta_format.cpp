#include "core/eta_format.h"

std::string formatEtaMinutes(int64_t targetEpoch, int64_t nowEpoch) {
    if (targetEpoch < 0) {
        return "--";
    }

    int64_t diffSeconds = targetEpoch - nowEpoch;
    int64_t minutes = (diffSeconds >= 0) ? (diffSeconds + 30) / 60
                                          : -((-diffSeconds + 30) / 60);

    if (minutes <= 0) {
        return "Due";
    }
    if (minutes > 60) {
        return "60+";
    }
    return std::to_string(minutes) + "m";
}
