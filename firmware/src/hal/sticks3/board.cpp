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
    /*arrivalsFontHeight=*/18,
    /*brightnessFull=*/128,
    /*brightnessDim=*/16,
    // Named on the side of the case, where it can be read while holding it.
    /*secondaryButtonLabel=*/"Btn B",
    // The case carries the name, so there is nothing an arrow would add.
    /*secondaryButtonArrow=*/ButtonArrow::None,
    // The PMIC reports a negative level with no battery attached, so an
    // absent battery is distinguishable from a full one.
    /*detectsBatteryPresence=*/true,
    // 135px leaves no room for window chrome on top of the rows.
    /*supportsFramedTheme=*/false,
};

}  // namespace

const BoardProfile& board() { return kProfile; }
