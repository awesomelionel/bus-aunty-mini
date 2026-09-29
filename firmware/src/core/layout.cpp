// firmware/src/core/layout.cpp
#include "core/layout.h"

namespace {

// The service number sits this far in from the left edge.
constexpr int kServiceColX = 4;
// Gutter kept clear at the right edge, and the height reserved at the bottom
// for the page indicator.
constexpr int kRightMargin = 4;
constexpr int kPageDotsBottomInset = 4;

static_assert(kArrivalsPerService > 1,
              "the ETA columns are spread between two anchors, so there have "
              "to be at least two of them");
static_assert(kShownArrivals >= 1 && kShownArrivals <= kArrivalsPerService,
              "the compact row draws a subset of the stored arrivals");

}  // namespace

ArrivalsLayout computeArrivalsLayout(int screenWidth, int screenHeight,
                                     int rowHeight, int batteryReservedWidth,
                                     bool hasLabels) {
    ArrivalsLayout layout{};
    layout.hasLabels = hasLabels;
    layout.serviceColX = kServiceColX;
    // Destination text shares the service number's line, so a labelled stop
    // uses the same pitch as one without labels.
    layout.rowHeight = rowHeight;

    // The header takes the first row, so one fewer than the rows that fit is
    // available for services. A row taller than the screen leaves none.
    if (layout.rowHeight > 0) {
        int rows = screenHeight / layout.rowHeight - 1;
        layout.servicesPerScreen = rows > 0 ? static_cast<size_t>(rows) : 0;
    }

    // The ETA block occupies the right half of the screen: the leftmost
    // column's right edge sits just short of the midpoint, leaving the left
    // half to the service number, and the rightmost ends kRightMargin from
    // the edge. The columns are spread at equal pitch across that span and
    // rounded to whole pixels, which at 240px reproduces the {119, 178, 236}
    // these values were before they were derived.
    // The old three-column spread still sets the pitch: leftmost just short
    // of the midpoint, rightmost kRightMargin from the edge. The compact row
    // keeps the rightmost kShownArrivals of those columns. Dropping the
    // leftmost one is what gives the destination its width.
    const int etaLeft = screenWidth / 2 - 1;
    const int etaRight = screenWidth - kRightMargin;
    const int span = etaRight - etaLeft;
    constexpr int steps = static_cast<int>(kArrivalsPerService) - 1;
    int spread[kArrivalsPerService];
    for (int i = 0; i < static_cast<int>(kArrivalsPerService); ++i) {
        spread[i] = etaLeft + (span * i + steps / 2) / steps;
    }
    const int skip = static_cast<int>(kArrivalsPerService - kShownArrivals);
    for (int i = 0; i < static_cast<int>(kShownArrivals); ++i) {
        layout.etaColRightX[i] = spread[skip + i];
    }

    layout.headerCenterX = (screenWidth - batteryReservedWidth) / 2;
    layout.pageDotsY = screenHeight - kPageDotsBottomInset;
    return layout;
}

int spreadRowPitch(int rowHeight, int listHeight, size_t rowCount,
                   size_t servicesPerScreen) {
    if (rowCount == 0 || rowCount >= servicesPerScreen || rowHeight <= 0) {
        return rowHeight;
    }
    int pitch = listHeight / static_cast<int>(rowCount);
    const int cap = rowHeight * 3 / 2;
    if (pitch > cap) {
        pitch = cap;
    }
    return pitch > rowHeight ? pitch : rowHeight;
}
