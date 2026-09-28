#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "board/board.h"
#include "core/arrival_parser.h"
#include "core/iso8601.h"
#include "core/night_window.h"
#include "hal/power.h"
#include "host/host_gfx.h"
#include "ui/display.h"

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<BusServiceRow> serviceRows(const std::vector<BusServiceRow>& rows,
                                       const std::string& service) {
    std::vector<BusServiceRow> kept;
    for (const BusServiceRow& row : rows) {
        if (row.serviceNo == service) {
            kept.push_back(row);
        }
    }
    return kept;
}

void shiftRows(std::vector<BusServiceRow>& rows, int64_t delta) {
    for (BusServiceRow& row : rows) {
        for (BusArrival& arrival : row.arrivals) {
            if (arrival.etaEpoch >= 0) {
                arrival.etaEpoch += delta;
            }
        }
    }
}

bool save(const std::string& path) {
    if (!hostSavePng(path.c_str())) {
        std::cerr << "failed to write " << path << "\n";
        return false;
    }
    std::cout << path << "\n";
    return true;
}

void prepare(const char* boardName, bool win95) {
    hostSetBoard(boardName);
    displaySetup();
    displaySetTheme(win95 && board().supportsFramedTheme);
}

hal::PowerStatus charged() {
    hal::PowerStatus power;
    power.percent = 80;
    power.charging = false;
    power.externalPower = false;
    return power;
}

bool rowsHaveLabels(const std::vector<BusServiceRow>& rows) {
    for (const BusServiceRow& row : rows) {
        if (rowShowsLabel(row)) {
            return true;
        }
    }
    return false;
}

bool drawArrivals(const std::string& path, const char* boardName, bool win95,
                  bool night, const std::string& title,
                  std::vector<BusServiceRow> rows, int64_t updatedAt,
                  uint32_t ageMs, size_t page, size_t pages, bool wifiOffline) {
    prepare(boardName, win95);
    int64_t now = updatedAt >= 0 ? updatedAt : 0;
    if (night) {
        constexpr int64_t kFourHours = 4 * 3600;
        now += kFourHours;
        shiftRows(rows, kFourHours);
        if (updatedAt >= 0) {
            updatedAt += kFourHours;
        }
    }
    pruneExpiredArrivals(rows, now);
    displayShowArrivals(title, rows, now, 0, 1, page, pages, charged(), ageMs,
                        updatedAt, wifiOffline);
    return save(path);
}

// Page 0 of the full stop, with the real page count, so the header dots
// sit in the reserved pad instead of on the stop name.
bool drawStopPage(const std::string& path, const char* boardName,
                  const ParsedBusStop& stop, const std::string& title) {
    prepare(boardName, false);
    const size_t per = servicesPerScreen(rowsHaveLabels(stop.rows));
    const size_t pages = servicePageCount(stop.rows, per);
    const std::vector<BusServiceRow> page = selectServicePage(stop.rows, per, 0);
    std::cout << boardName << " pages " << pages << " rows " << page.size()
              << "\n";
    return drawArrivals(path, boardName, false, false, title, page,
                        stop.updatedAtEpoch, 0, 0, pages, false);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : "/opt/cursor/artifacts";
    const std::string data = argc > 2 ? argv[2] : "test/data";

    const int marker = hostTextWidth(&fonts::DejaVu9, " 2nd");
    const int h18 = hostFontHeight(&fonts::DejaVu18);
    const int h24 = hostFontHeight(&fonts::DejaVu24);
    std::cout << "DejaVu9 ' 2nd' width " << marker << ", DejaVu18 height " << h18
              << ", DejaVu24 height " << h24 << "\n";
    if (marker <= 0 || h18 <= 0 || h24 <= 0) {
        std::cerr << "font metrics did not load\n";
        return 1;
    }

    const ParsedBusStop stop52109 =
        parseBusArrivalResponse(readFile(data + "/stop_52109.json"), "52109");
    const ParsedBusStop stop52109Dd = parseBusArrivalResponse(
        readFile(data + "/stop_52109_125dd.json"), "52109");
    const ParsedBusStop stop66271 =
        parseBusArrivalResponse(readFile(data + "/stop_66271.json"), "66271");
    const ParsedBusStop term =
        parseBusArrivalResponse(readFile(data + "/terminating.json"), "52109");
    if (!stop52109.valid || !stop52109Dd.valid || !stop66271.valid ||
        !term.valid) {
        std::cerr << "fixtures did not parse\n";
        return 1;
    }

    bool ok = true;
    ok &= drawStopPage(out + "/52109_3page_dots_sticks3.png", "sticks3",
                       stop52109, "Opp St. Michael's");
    ok &= drawStopPage(out + "/52109_3page_dots_tdisplay.png", "tdisplay",
                       stop52109, "Opp St. Michael's");
    ok &= drawStopPage(out + "/52109_3page_dots_feather.png", "feather",
                       stop52109, "Opp St. Michael's");

    const std::vector<BusServiceRow> loop125Dd =
        serviceRows(stop52109Dd.rows, "125");
    ok &= drawArrivals(out + "/125_win95_day_dd.png", "tdisplay", true, false,
                       "Opp St. Michael's", loop125Dd, stop52109Dd.updatedAtEpoch,
                       0, 0, 1, false);
    ok &= drawArrivals(out + "/125_win95_night_dd.png", "tdisplay", true, true,
                       "Opp St. Michael's", loop125Dd, stop52109Dd.updatedAtEpoch,
                       0, 0, 1, false);

    const std::vector<BusServiceRow> loop125 = serviceRows(stop52109.rows, "125");
    ok &= drawArrivals(out + "/stale_updatedat_sticks3.png", "sticks3", false,
                       false, "Opp St. Michael's", loop125, stop52109.updatedAtEpoch,
                       180000, 0, 1, false);
    ok &= drawArrivals(out + "/stale_win95_day.png", "tdisplay", true, false,
                       "Opp St. Michael's", loop125, stop52109.updatedAtEpoch,
                       180000, 0, 1, false);

    ok &= drawArrivals(out + "/wifi_off_sticks3.png", "sticks3", false, false,
                       "Opp St. Michael's", loop125, stop52109.updatedAtEpoch, 0,
                       0, 1, true);
    ok &= drawArrivals(out + "/wifi_off_win95_day.png", "tdisplay", true, false,
                       "Opp St. Michael's", loop125, stop52109.updatedAtEpoch, 0,
                       0, 1, true);

    ok &= drawArrivals(out + "/125_sticks3.png", "sticks3", false, false, "52109",
                       loop125, stop52109.updatedAtEpoch, 0, 0, 1, false);
    ok &= drawArrivals(out + "/136_66271_sticks3.png", "sticks3", false, false,
                       "66271", serviceRows(stop66271.rows, "136"),
                       stop66271.updatedAtEpoch, 0, 0, 1, false);
    ok &= drawArrivals(out + "/terminating_sticks3.png", "sticks3", false, false,
                       "52109", term.rows, term.updatedAtEpoch, 0, 0, 1, false);

    return ok ? 0 : 1;
}
