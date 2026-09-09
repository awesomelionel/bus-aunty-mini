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
    /*secondaryButtonLabel=*/"D0",
    // The D0/D1/D2 silkscreen is on the back of the board, so a user looking
    // at the screen cannot tell which button is which. At this rotation the
    // three sit along the bottom edge with D0 among them, so the prompt
    // points down instead of naming it.
    /*secondaryButtonHint=*/ButtonHint::ArrowDown,
    // D2 sleeps on a single press.
    /*hasSleepButton=*/true,
    // The MAX17048 reads the charger rail when no battery is attached and
    // reports a healthy charge, so an absent battery is indistinguishable
    // from a full one.
    /*detectsBatteryPresence=*/false,
};

}  // namespace

const BoardProfile& board() { return kProfile; }
