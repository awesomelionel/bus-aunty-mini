// firmware/src/ui/display.cpp
#include "ui/display.h"

#include <vector>

#include "board/board.h"
#include "core/eta_format.h"
#include "core/layout.h"
#include "hal/display_device.h"
#include "ui/wifi_image.h"

namespace {

// Bound to the panel here rather than in displaySetup() because M5Canvas
// takes its PSRAM preference from the parent at construction. The sprite
// itself is not allocated until createSprite() below, and neither this nor
// hal::gfx() touches the hardware, so binding at static-init is safe.
hal::Canvas canvas(&hal::gfx());

// Measured once in displaySetup(), because the row height it is derived from
// needs the arrivals font, and a font needs a live device.
ArrivalsLayout arrivalsLayout;

int screenWidth() { return board().screenWidth; }
int screenHeight() { return board().screenHeight; }

constexpr int kDefaultTextFont = 2;  // 16px; see displayShowWifiSetup

// Each arrival is tinted by how full that bus is. Colour carries the load and
// nothing else, so an arriving bus is left to read as "Arr" on its own.
uint16_t loadColor(BusLoad load) {
    switch (load) {
        case BusLoad::SeatsAvailable:
            return TFT_GREEN;
        case BusLoad::StandingAvailable:
            return TFT_ORANGE;
        case BusLoad::LimitedStanding:
            return TFT_RED;
        case BusLoad::Unknown:
            break;
    }
    return TFT_WHITE;
}

// The 96x96 source bitmap has blank padding around the glyph; only rows
// 16..83 carry ink, so the rest is cropped before scaling.
constexpr int kWifiIconRowBytes = WIFIIMAGE_WIDTH / 8;
constexpr int kWifiIconInkTop = 16;
constexpr int kWifiIconInkHeight = 68;
constexpr float kWifiIconScale = 0.25f;
constexpr int kWifiIconTextGap = 12;

// The service rows and the header end a few pixels short of the bottom edge,
// leaving room for an indicator that shows which page of a long service list
// is on screen.
void drawPageDots(size_t currentPage, size_t totalPages) {
    constexpr int kDotRadius = 2;
    constexpr int kDotSpacing = 8;

    int y = arrivalsLayout.pageDotsY;
    int x = screenWidth() / 2 -
            (static_cast<int>(totalPages - 1) * kDotSpacing) / 2;
    for (size_t i = 0; i < totalPages; ++i) {
        if (i == currentPage) {
            canvas.fillCircle(x, y, kDotRadius, TFT_WHITE);
        } else {
            canvas.drawCircle(x, y, kDotRadius, TFT_WHITE);
        }
        x += kDotSpacing;
    }
}

constexpr int kBatteryBodyWidth = 20;
constexpr int kBatteryHeight = 11;
constexpr int kBatteryTipWidth = 2;
constexpr int kBatteryTipHeight = 5;
constexpr int kBatteryRightMargin = 2;
constexpr int kBatteryY = 2;
constexpr int kBatteryLowPercent = 20;
// The header is centred in what the battery icon leaves free, so a long stop
// name cannot run underneath it.
constexpr int kHeaderRightPad =
    kBatteryBodyWidth + kBatteryTipWidth + kBatteryRightMargin + 4;

void drawBattery(const hal::PowerStatus& power) {
    if (power.percent < 0) {
        return;  // nothing read yet, or no gauge on this board
    }

    int x = screenWidth() - kBatteryBodyWidth - kBatteryTipWidth -
            kBatteryRightMargin;
    canvas.drawRect(x, kBatteryY, kBatteryBodyWidth, kBatteryHeight, TFT_WHITE);
    canvas.fillRect(x + kBatteryBodyWidth,
                    kBatteryY + (kBatteryHeight - kBatteryTipHeight) / 2,
                    kBatteryTipWidth, kBatteryTipHeight, TFT_WHITE);

    if (power.percent == 0) {
        return;
    }
    int innerWidth = kBatteryBodyWidth - 2;
    // Keep a sliver visible at low percentages so it stays distinguishable
    // from an empty outline.
    int fillWidth = (innerWidth * power.percent + 50) / 100;
    if (fillWidth < 1) {
        fillWidth = 1;
    }

    uint16_t color = TFT_WHITE;
    if (power.charging) {
        color = TFT_GREEN;
    } else if (power.percent <= kBatteryLowPercent) {
        color = TFT_RED;
    }
    canvas.fillRect(x + 1, kBatteryY + 1, fillWidth, kBatteryHeight - 2, color);
}

}  // namespace

void displaySetup() {
    hal::displayDeviceBegin();

    canvas.setColorDepth(8);
    canvas.createSprite(screenWidth(), screenHeight());

    // The arrivals rows are DejaVu18, so the layout is measured with that font
    // selected before the shared default goes back on.
    canvas.setFont(&fonts::DejaVu18);
    arrivalsLayout = computeArrivalsLayout(screenWidth(), screenHeight(),
                                           canvas.fontHeight(),
                                           kHeaderRightPad);

    canvas.setTextFont(kDefaultTextFont);
    canvas.setTextSize(1);
}

size_t servicesPerScreen() { return arrivalsLayout.servicesPerScreen; }

void displaySetDimmed(bool dimmed) {
    hal::displayDeviceSetBrightness(dimmed ? board().brightnessDim
                                           : board().brightnessFull);
}

void displaySleep() {
    // Clear before sleeping: the panel keeps its own frame buffer, so whatever
    // was last pushed would otherwise flash back up on wake, showing arrival
    // times that are by then minutes stale.
    canvas.fillSprite(TFT_BLACK);
    canvas.pushSprite(0, 0);
    hal::displayDeviceSleep();
}

void displayWake() { hal::displayDeviceWake(); }

void displayShowStatus(const std::string& message) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextDatum(middle_center);

    std::vector<std::string> lines;
    size_t start = 0;
    while (true) {
        size_t pos = message.find('\n', start);
        if (pos == std::string::npos) {
            lines.push_back(message.substr(start));
            break;
        }
        lines.push_back(message.substr(start, pos - start));
        start = pos + 1;
    }

    int lineHeight = canvas.fontHeight();
    int totalHeight = lineHeight * static_cast<int>(lines.size());
    int firstLineY = (screenHeight() - totalHeight) / 2 + lineHeight / 2;

    for (size_t i = 0; i < lines.size(); ++i) {
        int y = firstLineY + static_cast<int>(i) * lineHeight;
        canvas.drawString(lines[i].c_str(), screenWidth() / 2, y);
    }

    canvas.pushSprite(0, 0);
}

void displayShowWifiSetup(const std::string& ssid) {
    canvas.fillSprite(TFT_BLACK);
    // 18px here rather than the shared 16px default. Font 2 is a bitmap font,
    // so scaling it to 18 would resample unevenly; DejaVu18 is natively 18px.
    canvas.setFont(&fonts::DejaVu18);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextDatum(top_center);

    int lineHeight = canvas.fontHeight();
    int iconHeight = static_cast<int>(kWifiIconInkHeight * kWifiIconScale + 0.5f);
    int blockHeight = iconHeight + kWifiIconTextGap + lineHeight * 2;
    int iconY = (screenHeight() - blockHeight) / 2;

    // Shrinking the arcs this far needs antialiasing, which only works from a
    // sprite source, so the cropped bitmap is staged before being zoomed down.
    hal::Canvas icon(&canvas);
    icon.setColorDepth(8);
    if (icon.createSprite(WIFIIMAGE_WIDTH, kWifiIconInkHeight)) {
        icon.fillSprite(TFT_BLACK);
        icon.drawBitmap(0, 0,
                        epd_bitmap_WifiImage + kWifiIconInkTop * kWifiIconRowBytes,
                        WIFIIMAGE_WIDTH, kWifiIconInkHeight, TFT_WHITE);
        icon.setPivot(WIFIIMAGE_WIDTH / 2.0f, kWifiIconInkHeight / 2.0f);
        icon.pushRotateZoomWithAA(&canvas, screenWidth() / 2.0f,
                                  iconY + iconHeight / 2.0f, 0.0f,
                                  kWifiIconScale, kWifiIconScale, TFT_BLACK);
        icon.deleteSprite();
    }

    int textY = iconY + iconHeight + kWifiIconTextGap;
    canvas.drawString("Connect WiFi to:", screenWidth() / 2, textY);
    canvas.drawString(ssid.c_str(), screenWidth() / 2, textY + lineHeight);

    canvas.pushSprite(0, 0);
    canvas.setTextFont(kDefaultTextFont);
}

void displayShowNoStops(const std::string& ssid) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextDatum(top_center);

    int lineHeight = canvas.fontHeight();
    const std::string holdLine =
        std::string("Hold ") + board().secondaryButtonLabel + ", then";
    const std::string lines[] = {"No bus stops yet", holdLine, "join WiFi:",
                                 ssid};
    int count = static_cast<int>(sizeof(lines) / sizeof(lines[0]));
    int firstLineY = (screenHeight() - lineHeight * count) / 2;

    for (int i = 0; i < count; ++i) {
        canvas.drawString(lines[i].c_str(), screenWidth() / 2,
                          firstLineY + i * lineHeight);
    }

    canvas.pushSprite(0, 0);
}

void displayShowArrivals(const std::string& stopLabel,
                          const std::vector<BusService>& services,
                          int64_t nowEpoch, size_t currentStopIndex,
                          size_t totalStops, size_t currentPage,
                          size_t totalPages, const hal::PowerStatus& power) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setFont(&fonts::DejaVu18);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);

    // The header takes the first row, leaving the layout's row count below it.
    const int rowHeight = arrivalsLayout.rowHeight;

    canvas.setTextDatum(top_center);
    std::string header = stopLabel + " (" +
                          std::to_string(currentStopIndex + 1) + "/" +
                          std::to_string(totalStops) + ")";
    canvas.drawString(header.c_str(), arrivalsLayout.headerCenterX, 0);

    drawBattery(power);

    for (size_t i = 0;
         i < services.size() && i < arrivalsLayout.servicesPerScreen; ++i) {
        int y = rowHeight + static_cast<int>(i) * rowHeight;
        const BusService& svc = services[i];

        canvas.setTextColor(TFT_WHITE, TFT_BLACK);
        canvas.setTextDatum(top_left);
        canvas.drawString(svc.serviceNo.c_str(), arrivalsLayout.serviceColX, y);

        canvas.setTextDatum(top_right);
        for (size_t col = 0; col < kArrivalsPerService; ++col) {
            const BusArrival& arrival = svc.arrivals[col];
            std::string eta = formatEtaMinutes(arrival.etaEpoch, nowEpoch);

            // Single argument leaves the text background transparent, which
            // the second pass below depends on. Safe because every frame
            // starts from a cleared sprite.
            canvas.setTextColor(loadColor(arrival.load));
            canvas.drawString(eta.c_str(), arrivalsLayout.etaColRightX[col], y);

            // Colour is spoken for by load, so an arriving bus is emphasised
            // by weight: overdrawing a pixel to the left thickens the stems.
            // Leftward because the columns are right-aligned, so that is
            // where the spare room is.
            if (eta == kEtaArrivingLabel) {
                canvas.drawString(eta.c_str(),
                                  arrivalsLayout.etaColRightX[col] - 1, y);
            }
        }
    }

    if (totalPages > 1) {
        drawPageDots(currentPage, totalPages);
    }

    canvas.pushSprite(0, 0);
    canvas.setTextFont(kDefaultTextFont);
}
