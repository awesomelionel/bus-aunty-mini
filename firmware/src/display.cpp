// firmware/src/display.cpp
#include "display.h"

#include <M5Unified.h>

#include "eta_format.h"

namespace {

M5Canvas canvas(&M5.Display);

constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;
constexpr int kRowHeight = 16;
constexpr int kFirstRowY = 16;
constexpr int kServiceColX = 4;
constexpr int kEtaColX[3] = {150, 195, 236};

}  // namespace

void displaySetup() {
    M5.Display.setRotation(1);
    canvas.setColorDepth(8);
    canvas.createSprite(kScreenWidth, kScreenHeight);
    canvas.setTextFont(2);
    canvas.setTextSize(1);
}

void displayShowStatus(const std::string& message) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextDatum(middle_center);
    canvas.drawString(message.c_str(), kScreenWidth / 2, kScreenHeight / 2);
    canvas.pushSprite(0, 0);
}

void displayShowArrivals(const std::string& busStopCode,
                          const std::vector<BusService>& services,
                          int64_t nowEpoch, size_t currentStopIndex,
                          size_t totalStops) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);

    canvas.setTextDatum(top_left);
    std::string header = busStopCode + " (" +
                          std::to_string(currentStopIndex + 1) + "/" +
                          std::to_string(totalStops) + ")";
    canvas.drawString(header.c_str(), kServiceColX, 0);

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
