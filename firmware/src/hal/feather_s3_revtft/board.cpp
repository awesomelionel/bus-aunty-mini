// firmware/src/hal/feather_s3_revtft/board.cpp
#include "board/board.h"

namespace {

constexpr BoardProfile kProfile = {
    /*name=*/"Reverse TFT Feather",
    /*screenWidth=*/240,
    /*screenHeight=*/135,
    // The panel is mounted "reversed" relative to the StickS3, so the
    // StickS3's rotation 1 comes out upside down here and 3 is the landscape
    // that puts the three buttons in a column beside the screen.
    /*rotation=*/3,
    // Same levels as the StickS3 to start with, so the two boards look alike.
    /*brightnessFull=*/128,
    /*brightnessDim=*/16,
    // The D0/D1/D2 silkscreen is on the back of the board, so naming the
    // button that way tells a user looking at the screen nothing. The three
    // sit in a column, so the middle one is identified by position instead.
    /*secondaryButtonLabel=*/"middle btn",
    // The MAX17048 reads the charger rail when no battery is attached and
    // reports a healthy charge, so an absent battery is indistinguishable
    // from a full one.
    /*detectsBatteryPresence=*/false,
};

}  // namespace

const BoardProfile& board() { return kProfile; }
