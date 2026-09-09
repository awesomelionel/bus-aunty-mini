// firmware/src/hal/sticks3/board.cpp
#include "board/board.h"

namespace {

// These are the values that were hardcoded in ui/display.cpp and main.cpp
// before the board layer existed; keeping them identical is what makes the
// refactor a no-op on this board.
constexpr BoardProfile kProfile = {
    /*name=*/"StickS3",
    /*screenWidth=*/240,
    /*screenHeight=*/135,
    /*rotation=*/1,
    /*brightnessFull=*/128,
    /*brightnessDim=*/16,
    /*primaryButtonLabel=*/"Btn A",
    /*secondaryButtonLabel=*/"Btn B",
    /*hasSleepButton=*/false,
    // The PMIC reports a negative level with no battery attached, so an
    // absent battery is distinguishable from a full one.
    /*detectsBatteryPresence=*/true,
};

}  // namespace

const BoardProfile& board() { return kProfile; }
