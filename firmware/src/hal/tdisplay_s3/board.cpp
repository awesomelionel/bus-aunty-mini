// firmware/src/hal/tdisplay_s3/board.cpp
#include "board/board.h"

namespace {

constexpr BoardProfile kProfile = {
    /*name=*/"T-Display-S3",
    // The first panel in the tree that is not 240x135. Everything on the
    // arrivals screen is derived from these, so the extra room turns into
    // extra service rows rather than empty space.
    /*screenWidth=*/320,
    /*screenHeight=*/170,
    // Landscape with the two buttons on the right, confirmed on device: 3 is
    // the same landscape turned 180 degrees and comes out upside down here.
    /*rotation=*/1,
    // The roomier panel can afford the next DejaVu size up, which is the
    // whole point of putting this board on a 1.9" screen.
    /*arrivalsFontHeight=*/24,
    // Same levels as the other two boards, so all three look alike.
    /*brightnessFull=*/128,
    /*brightnessDim=*/16,
    // Named for the silkscreen rather than by position, because that holds
    // whichever way up the board ends up being read -- unlike the Feather,
    // whose labels are on the back, BOOT is marked on this one.
    /*secondaryButtonLabel=*/"BOOT btn",
    // The two buttons sit side by side with BOOT on the left, confirmed on
    // device, so pointing left identifies it without the user having to find
    // any silkscreen at all.
    /*secondaryButtonArrow=*/ButtonArrow::Left,
    // The ADC reads well above a full cell when no battery is attached, so
    // an absent battery is distinguishable from a charged one. See power.cpp.
    /*detectsBatteryPresence=*/true,
    // 170px carries the chrome and still shows four services.
    /*supportsFramedTheme=*/true,
};

}  // namespace

const BoardProfile& board() { return kProfile; }
