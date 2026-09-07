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
constexpr int kRowHeight = 16;
constexpr int kFirstRowY = 16;
constexpr int kServiceColX = 4;
constexpr int kEtaColX[3] = {150, 195, 236};
constexpr int kDefaultTextFont = 2;  // 16px; see displayShowWifiSetup

// The 96x96 source bitmap has blank padding around the glyph; only rows
// 16..83 carry ink, so the rest is cropped before scaling.
constexpr int kWifiIconRowBytes = WIFIIMAGE_WIDTH / 8;
constexpr int kWifiIconInkTop = 16;
constexpr int kWifiIconInkHeight = 68;
constexpr float kWifiIconScale = 0.25f;
constexpr int kWifiIconTextGap = 12;

}  // namespace

void displaySetup() {
    M5.Display.setRotation(1);
    canvas.setColorDepth(8);
    canvas.createSprite(kScreenWidth, kScreenHeight);
    canvas.setTextFont(kDefaultTextFont);
    canvas.setTextSize(1);
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
                          size_t totalStops) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);

    canvas.setTextDatum(top_center);
    std::string header = stopLabel + " (" +
                          std::to_string(currentStopIndex + 1) + "/" +
                          std::to_string(totalStops) + ")";
    canvas.drawString(header.c_str(), kScreenWidth / 2, 0);

    for (size_t i = 0; i < services.size() && i < 6; ++i) {
        int y = kFirstRowY + static_cast<int>(i) * kRowHeight;
        const BusService& svc = services[i];

        canvas.setTextDatum(top_left);
        canvas.drawString(svc.serviceNo.c_str(), kServiceColX, y);

        canvas.setTextDatum(top_right);
        canvas.drawString(
            formatEtaMinutes(svc.times.eta1Epoch, nowEpoch).c_str(),
            kEtaColX[0], y);
        canvas.drawString(
            formatEtaMinutes(svc.times.eta2Epoch, nowEpoch).c_str(),
            kEtaColX[1], y);
        canvas.drawString(
            formatEtaMinutes(svc.times.eta3Epoch, nowEpoch).c_str(),
            kEtaColX[2], y);
    }

    canvas.pushSprite(0, 0);
}
