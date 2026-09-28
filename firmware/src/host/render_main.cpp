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

bool drawArrivals(const std::string& path, const char* boardName, bool win95,
                  bool night, const std::string& title,
                  std::vector<BusServiceRow> rows, int64_t updatedAt,
                  uint32_t ageMs, size_t page, size_t pages) {
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
                        updatedAt, false);
    return save(path);
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
    const ParsedBusStop stop52049 =
        parseBusArrivalResponse(readFile(data + "/stop_52049.json"), "52049");
    if (!stop52109.valid || !stop52049.valid) {
        std::cerr << "live fixtures did not parse\n";
        return 1;
    }

    bool ok = true;
    const std::vector<BusServiceRow> loop125 =
        serviceRows(stop52109.rows, "125");
    const char* boards[] = {"sticks3", "feather", "tdisplay"};
    for (const char* name : boards) {
        ok &= drawArrivals(out + "/125_" + name + ".png", name, false, false,
                           "52109", loop125, stop52109.updatedAtEpoch, 0, 0, 1);
    }
    ok &= drawArrivals(out + "/125_tdisplay_win95_day.png", "tdisplay", true, false,
                       "Opp St. Michael's", loop125, stop52109.updatedAtEpoch, 0, 0,
                       1);
    ok &= drawArrivals(out + "/125_tdisplay_win95_night.png", "tdisplay", true, true,
                       "Opp St. Michael's", loop125, stop52109.updatedAtEpoch, 0, 0,
                       1);

    ok &= drawArrivals(out + "/124_sticks3.png", "sticks3", false, false, "52109",
                       serviceRows(stop52109.rows, "124"), stop52109.updatedAtEpoch,
                       0, 0, 1);
    ok &= drawArrivals(out + "/124_tdisplay.png", "tdisplay", false, false, "52109",
                       serviceRows(stop52109.rows, "124"), stop52109.updatedAtEpoch,
                       0, 0, 1);

    std::vector<BusServiceRow> both;
    const std::vector<BusServiceRow> s21 = serviceRows(stop52049.rows, "21");
    const std::vector<BusServiceRow> s129 = serviceRows(stop52049.rows, "129");
    both.insert(both.end(), s21.begin(), s21.end());
    both.insert(both.end(), s129.begin(), s129.end());
    ok &= drawArrivals(out + "/21_129_sticks3.png", "sticks3", false, false, "52049",
                       both, stop52049.updatedAtEpoch, 0, 0, 1);
    ok &= drawArrivals(out + "/21_129_tdisplay.png", "tdisplay", false, false, "52049",
                       both, stop52049.updatedAtEpoch, 0, 0, 1);

    const ParsedBusStop figure =
        parseBusArrivalResponse(readFile(data + "/figure_eight.json"), "54009");
    if (!figure.valid) {
        std::cerr << "figure-eight fixture did not parse\n";
        return 1;
    }
    ok &= drawArrivals(out + "/figure_eight_sticks3.png", "sticks3", false, false,
                       "54009", figure.rows, figure.updatedAtEpoch, 0, 0, 2);
    ok &= drawArrivals(out + "/figure_eight_tdisplay.png", "tdisplay", false, false,
                       "54009", figure.rows, figure.updatedAtEpoch, 0, 0, 2);

    const ParsedBusStop trunc =
        parseBusArrivalResponse(readFile(data + "/truncate_visit2.json"), "52109");
    ok &= drawArrivals(out + "/truncate_2nd_sticks3.png", "sticks3", false, false,
                       "52109", trunc.rows, trunc.updatedAtEpoch, 0, 0, 1);

    // The Prince Edward label above fits (DejaVu9 is 161px, 203px with the
    // marker reserved). This longer one is the case that actually cuts.
    const ParsedBusStop cut =
        parseBusArrivalResponse(readFile(data + "/truncate_long.json"), "52109");
    ok &= drawArrivals(out + "/truncate_cut_sticks3.png", "sticks3", false, false,
                       "52109", cut.rows, cut.updatedAtEpoch, 0, 0, 1);

    const ParsedBusStop term =
        parseBusArrivalResponse(readFile(data + "/terminating.json"), "52109");
    ok &= drawArrivals(out + "/terminating_sticks3.png", "sticks3", false, false,
                       "52109", term.rows, term.updatedAtEpoch, 0, 0, 1);
    ok &= drawArrivals(out + "/terminating_feather.png", "feather", false, false,
                       "52109", term.rows, term.updatedAtEpoch, 0, 0, 1);
    ok &= drawArrivals(out + "/terminating_tdisplay.png", "tdisplay", false, false,
                       "52109", term.rows, term.updatedAtEpoch, 0, 0, 1);
    ok &= drawArrivals(out + "/terminating_win95_day.png", "tdisplay", true, false,
                       "52109", term.rows, term.updatedAtEpoch, 0, 0, 1);
    ok &= drawArrivals(out + "/terminating_win95_night.png", "tdisplay", true, true,
                       "52109", term.rows, term.updatedAtEpoch, 0, 0, 1);

    ok &= drawArrivals(out + "/stale_header_sticks3.png", "sticks3", false, false,
                       "Befname Tampines", loop125, stop52109.updatedAtEpoch, 180000,
                       1, 3);
    ok &= drawArrivals(out + "/stale_header_win95_day.png", "tdisplay", true, false,
                       "Befname Tampines", loop125, stop52109.updatedAtEpoch, 180000,
                       1, 3);

    prepare("sticks3", false);
    displayShowStatus("No data\nLast: " + formatLocalHm(stop52109.updatedAtEpoch));
    ok &= save(out + "/no_data_last_sticks3.png");

    prepare("sticks3", false);
    displayShowStatus("No More Buses\nUpdated " +
                      formatLocalHm(stop52109.updatedAtEpoch));
    ok &= save(out + "/no_more_buses_sticks3.png");

    prepare("tdisplay", true);
    displayShowStatus("No data\nLast: " + formatLocalHm(stop52109.updatedAtEpoch));
    ok &= save(out + "/no_data_last_tdisplay.png");

    return ok ? 0 : 1;
}
