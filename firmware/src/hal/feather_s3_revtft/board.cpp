// firmware/src/hal/feather_s3_revtft/board.cpp
#include "board/board.h"

namespace {

constexpr BoardProfile kProfile = {
    /*name=*/"Reverse TFT Feather",
    /*screenWidth=*/240,
    /*screenHeight=*/135,
    // The panel is mounted "reversed" relative to the StickS3, so the
    // StickS3's rotation 1 comes out upside down here and 3 is the landscape
    // that puts the three buttons along the bottom edge.
    /*rotation=*/3,
    // Same levels as the StickS3 to start with, so the two boards look alike.
    /*brightnessFull=*/128,
    /*brightnessDim=*/16,
    /*primaryButtonLabel=*/"D1",
    /*secondaryButtonLabel=*/"D2",
    // D0 sleeps on a single press; it is the BOOT pin, so it gets no hold
    // gesture. See src/hal/feather_s3_revtft/buttons.cpp.
    /*hasSleepButton=*/true,
    // The MAX17048 reads the charger rail when no battery is attached and
    // reports a healthy charge, so an absent battery is indistinguishable
    // from a full one.
    /*detectsBatteryPresence=*/false,
};

}  // namespace

const BoardProfile& board() { return kProfile; }
