// firmware/src/hal/sticks3/m5_init.cpp
#include "m5_init.h"

#include <M5Unified.h>

namespace hal::sticks3 {

void ensureM5Begun() {
    static bool begun = false;
    if (begun) {
        return;
    }
    begun = true;

    auto cfg = M5.config();
    M5.begin(cfg);
}

}  // namespace hal::sticks3
