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
    // kShownArrivals wide: the rightmost columns of the old three-column
    // spread, so the dropped column's width is free for the destination.
    int etaColRightX[kShownArrivals];
    int headerCenterX;  // centred in what the battery icon leaves free
    int pageDotsY;
    // Recorded from the caller. Labels share the service line, so this does
    // not change row pitch or how many services fit.
    bool hasLabels;
};

// `rowHeight` is passed in rather than measured here, because measuring needs
// a font and a font needs a device. `batteryReservedWidth` is the horizontal
// space the battery icon occupies at the top right, which the header is
// centred clear of. `hasLabels` is stored and does not change spacing: the
// destination is drawn on the service number's line.
ArrivalsLayout computeArrivalsLayout(int screenWidth, int screenHeight,
                                     int rowHeight, int batteryReservedWidth,
                                     bool hasLabels);

// Row pitch for a page of `rowCount` services in a list `listHeight` tall.
// A full page keeps `rowHeight`. A short one -- the pager keeps a service's
// rows together, so a page can end a row or more early -- spreads its rows
// over the list, capped at half a row extra so a page of one or two does
// not scatter down the screen.
int spreadRowPitch(int rowHeight, int listHeight, size_t rowCount,
                   size_t servicesPerScreen);
