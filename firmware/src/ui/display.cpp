// firmware/src/ui/display.cpp
#include "ui/display.h"

#include <M5Unified.h>

#include <vector>

#include "core/eta_format.h"
#include "ui/wifi_image.h"

namespace {

M5Canvas canvas(&M5.Display);

constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;
constexpr int kServiceColX = 4;
// Right edges of the three ETA columns, spread across the space left after
// the service number rather than packed against the right edge. At 18px the
// widest ETA ("60+") is 40px, so this leaves an 18px gutter between columns.
constexpr int kEtaColX[kArrivalsPerService] = {119, 178, 236};
constexpr int kDefaultTextFont = 2;  // 16px; see displayShowWifiSetup

// Backlight level, 0-255. Set explicitly at boot rather than left to
// M5Unified's default so that waking has a known level to return to.
constexpr uint8_t kBrightnessFull = 128;

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

// Six 18px rows plus the header end at y=126, leaving 9px for an indicator
// that shows which page of a long service list is on screen.
void drawPageDots(size_t currentPage, size_t totalPages) {
    constexpr int kDotRadius = 2;
    constexpr int kDotSpacing = 8;

    int y = kScreenHeight - kDotRadius - 2;
    int x = kScreenWidth / 2 -
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

void drawBattery(const BatteryReading& battery) {
    if (battery.percent < 0) {
        return;  // running off USB with no battery attached
    }

    int x = kScreenWidth - kBatteryBodyWidth - kBatteryTipWidth -
            kBatteryRightMargin;
    canvas.drawRect(x, kBatteryY, kBatteryBodyWidth, kBatteryHeight, TFT_WHITE);
    canvas.fillRect(x + kBatteryBodyWidth,
                    kBatteryY + (kBatteryHeight - kBatteryTipHeight) / 2,
                    kBatteryTipWidth, kBatteryTipHeight, TFT_WHITE);

    if (battery.percent == 0) {
        return;
    }
    int innerWidth = kBatteryBodyWidth - 2;
    // Keep a sliver visible at low percentages so it stays distinguishable
    // from an empty outline.
    int fillWidth = (innerWidth * battery.percent + 50) / 100;
    if (fillWidth < 1) {
        fillWidth = 1;
    }

    uint16_t color = TFT_WHITE;
    if (battery.charging) {
        color = TFT_GREEN;
    } else if (battery.percent <= kBatteryLowPercent) {
        color = TFT_RED;
    }
    canvas.fillRect(x + 1, kBatteryY + 1, fillWidth, kBatteryHeight - 2, color);
}

}  // namespace

void displaySetup() {
    M5.Display.setRotation(1);
    M5.Display.setBrightness(kBrightnessFull);
    canvas.setColorDepth(8);
    canvas.createSprite(kScreenWidth, kScreenHeight);
    canvas.setTextFont(kDefaultTextFont);
    canvas.setTextSize(1);
}

void displaySleep() {
    // Clear before sleeping: the panel keeps its own frame buffer, so whatever
    // was last pushed would otherwise flash back up on wake, showing arrival
    // times that are by then minutes stale.
    canvas.fillSprite(TFT_BLACK);
    canvas.pushSprite(0, 0);
    // Takes the backlight to zero itself, and remembers the level to restore.
    // Setting the brightness to 0 here instead would make that remembered
    // level 0, and the panel would wake up black.
    M5.Display.sleep();
}

void displayWake() {
    M5.Display.wakeup();
    // wakeup() restores the level remembered from before the sleep; set it
    // explicitly anyway so the panel cannot come back at some other level.
    M5.Display.setBrightness(kBrightnessFull);
}

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
    int firstLineY = (kScreenHeight - totalHeight) / 2 + lineHeight / 2;

    for (size_t i = 0; i < lines.size(); ++i) {
        int y = firstLineY + static_cast<int>(i) * lineHeight;
        canvas.drawString(lines[i].c_str(), kScreenWidth / 2, y);
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
    int iconY = (kScreenHeight - blockHeight) / 2;

    // Shrinking the arcs this far needs antialiasing, which only works from a
    // sprite source, so the cropped bitmap is staged before being zoomed down.
    M5Canvas icon(&canvas);
    icon.setColorDepth(8);
    if (icon.createSprite(WIFIIMAGE_WIDTH, kWifiIconInkHeight)) {
        icon.fillSprite(TFT_BLACK);
        icon.drawBitmap(0, 0,
                        epd_bitmap_WifiImage + kWifiIconInkTop * kWifiIconRowBytes,
                        WIFIIMAGE_WIDTH, kWifiIconInkHeight, TFT_WHITE);
        icon.setPivot(WIFIIMAGE_WIDTH / 2.0f, kWifiIconInkHeight / 2.0f);
        icon.pushRotateZoomWithAA(&canvas, kScreenWidth / 2.0f,
                                  iconY + iconHeight / 2.0f, 0.0f,
                                  kWifiIconScale, kWifiIconScale, TFT_BLACK);
        icon.deleteSprite();
    }

    int textY = iconY + iconHeight + kWifiIconTextGap;
    canvas.drawString("Connect WiFi to:", kScreenWidth / 2, textY);
    canvas.drawString(ssid.c_str(), kScreenWidth / 2, textY + lineHeight);

    canvas.pushSprite(0, 0);
    canvas.setTextFont(kDefaultTextFont);
}

void displayShowNoStops(const std::string& ssid) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextDatum(top_center);

    int lineHeight = canvas.fontHeight();
    const std::string lines[] = {"No bus stops yet", "Hold Btn B, then",
                                 "join WiFi:", ssid};
    int count = static_cast<int>(sizeof(lines) / sizeof(lines[0]));
    int firstLineY = (kScreenHeight - lineHeight * count) / 2;

    for (int i = 0; i < count; ++i) {
        canvas.drawString(lines[i].c_str(), kScreenWidth / 2,
                          firstLineY + i * lineHeight);
    }

    canvas.pushSprite(0, 0);
}

void displayShowArrivals(const std::string& stopLabel,
                          const std::vector<BusService>& services,
                          int64_t nowEpoch, size_t currentStopIndex,
                          size_t totalStops, size_t currentPage,
                          size_t totalPages, const BatteryReading& battery) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setFont(&fonts::DejaVu18);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);

    // The header takes the first row, leaving exactly kServicesPerScreen rows.
    int rowHeight = canvas.fontHeight();

    canvas.setTextDatum(top_center);
    std::string header = stopLabel + " (" +
                          std::to_string(currentStopIndex + 1) + "/" +
                          std::to_string(totalStops) + ")";
    canvas.drawString(header.c_str(), (kScreenWidth - kHeaderRightPad) / 2, 0);

    drawBattery(battery);

    for (size_t i = 0; i < services.size() && i < kServicesPerScreen; ++i) {
        int y = rowHeight + static_cast<int>(i) * rowHeight;
        const BusService& svc = services[i];

        canvas.setTextColor(TFT_WHITE, TFT_BLACK);
        canvas.setTextDatum(top_left);
        canvas.drawString(svc.serviceNo.c_str(), kServiceColX, y);

        canvas.setTextDatum(top_right);
        for (size_t col = 0; col < kArrivalsPerService; ++col) {
            const BusArrival& arrival = svc.arrivals[col];
            std::string eta = formatEtaMinutes(arrival.etaEpoch, nowEpoch);

            // Single argument leaves the text background transparent, which
            // the second pass below depends on. Safe because every frame
            // starts from a cleared sprite.
            canvas.setTextColor(loadColor(arrival.load));
            canvas.drawString(eta.c_str(), kEtaColX[col], y);

            // Colour is spoken for by load, so an arriving bus is emphasised
            // by weight: overdrawing a pixel to the left thickens the stems.
            // Leftward because the columns are right-aligned, so that is
            // where the spare room is.
            if (eta == kEtaArrivingLabel) {
                canvas.drawString(eta.c_str(), kEtaColX[col] - 1, y);
            }
        }
    }

    if (totalPages > 1) {
        drawPageDots(currentPage, totalPages);
    }

    canvas.pushSprite(0, 0);
    canvas.setTextFont(kDefaultTextFont);
}
