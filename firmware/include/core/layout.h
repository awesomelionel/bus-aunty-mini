// firmware/include/core/layout.h
#pragma once
#include <cstddef>

#include "core/arrival_parser.h"

// Where everything sits on the arrivals screen, derived from the panel's
// geometry rather than fixed to the 240x135 the first supported board
// happened to have.
struct ArrivalsLayout {
    size_t servicesPerScreen;  // rows that fit under the header
    int rowHeight;
    int serviceColX;
    // Right edges of the ETA columns; the text is right-aligned to these.
    int etaColRightX[kArrivalsPerService];
    int headerCenterX;  // centred in what the battery icon leaves free
    int pageDotsY;
};

// `rowHeight` is passed in rather than measured here, because measuring needs
// a font and a font needs a device. `batteryReservedWidth` is the horizontal
// space the battery icon occupies at the top right, which the header is
// centred clear of.
ArrivalsLayout computeArrivalsLayout(int screenWidth, int screenHeight,
                                     int rowHeight, int batteryReservedWidth);
