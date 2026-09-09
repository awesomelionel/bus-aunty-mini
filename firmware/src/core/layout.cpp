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

}  // namespace

ArrivalsLayout computeArrivalsLayout(int screenWidth, int screenHeight,
                                     int rowHeight, int batteryReservedWidth) {
    ArrivalsLayout layout{};
    layout.rowHeight = rowHeight;
    layout.serviceColX = kServiceColX;

    // The header takes the first row, so one fewer than the rows that fit is
    // available for services. A row taller than the screen leaves none.
    if (rowHeight > 0) {
        int rows = screenHeight / rowHeight - 1;
        layout.servicesPerScreen = rows > 0 ? static_cast<size_t>(rows) : 0;
    }

    // The ETA block occupies the right half of the screen: the leftmost
    // column's right edge sits just short of the midpoint, leaving the left
    // half to the service number, and the rightmost ends kRightMargin from
    // the edge. The columns are spread at equal pitch across that span and
    // rounded to whole pixels, which at 240px reproduces the {119, 178, 236}
    // these values were before they were derived.
    const int etaLeft = screenWidth / 2 - 1;
    const int etaRight = screenWidth - kRightMargin;
    const int span = etaRight - etaLeft;
    constexpr int steps = static_cast<int>(kArrivalsPerService) - 1;
    for (int i = 0; i < static_cast<int>(kArrivalsPerService); ++i) {
        layout.etaColRightX[i] = etaLeft + (span * i + steps / 2) / steps;
    }

    layout.headerCenterX = (screenWidth - batteryReservedWidth) / 2;
    layout.pageDotsY = screenHeight - kPageDotsBottomInset;
    return layout;
}
