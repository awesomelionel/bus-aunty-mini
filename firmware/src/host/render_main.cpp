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

// What the firmware can actually show. Win95 is T-Display only
// (supportsFramedTheme). Night is the framed palette only: the plain path
// never calls isNightAt. Passing Win95 on StickS3 or Feather stays plain,
// so those combinations are not rendered.
struct View {
    const char* board;
    bool win95;
    bool night;
    const char* tag;
};

constexpr View kViews[] = {
    {"sticks3", false, false, "sticks3_plain"},
    {"feather", false, false, "feather_plain"},
    {"tdisplay", false, false, "tdisplay_plain"},
    {"tdisplay", true, false, "tdisplay_win95_day"},
    {"tdisplay", true, true, "tdisplay_win95_night"},
};

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

bool drawEach(const std::string& out, const std::string& scenario,
              const std::string& title, std::vector<BusServiceRow> rows,
              int64_t updatedAt, uint32_t ageMs, bool wifiOffline) {
    bool ok = true;
    for (const View& view : kViews) {
        ok &= drawArrivals(out + "/" + scenario + "_" + view.tag + ".png",
                           view.board, view.win95, view.night, title, rows,
                           updatedAt, ageMs, 0, 1, wifiOffline);
    }
    return ok;
}

// Page 0 of the full stop. Page count is measured after the theme is set,
// because Win95 chrome fits fewer rows than the plain list.
bool drawStopPages(const std::string& out, const std::string& scenario,
                   const ParsedBusStop& stop, const std::string& title) {
    bool ok = true;
    for (const View& view : kViews) {
        prepare(view.board, view.win95);
        const size_t per = servicesPerScreen(rowsHaveLabels(stop.rows));
        const size_t pages = servicePageCount(stop.rows, per);
        const std::vector<BusServiceRow> page =
            selectServicePage(stop.rows, per, 0);
        std::cout << scenario << " " << view.tag << " pages " << pages
                  << " rows " << page.size() << "\n";
        ok &= drawArrivals(out + "/" + scenario + "_" + view.tag + ".png",
                           view.board, view.win95, view.night, title, page,
                           stop.updatedAtEpoch, 0, 0, pages, false);
    }
    return ok;
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
    ok &= drawStopPages(out, "dots", stop52109, "Opp St. Michael's");

    // The 17:18 capture is the one where 125 visit 2 is a double-decker.
    // The plain theme does not draw that mark; Win95 does, on the ETA line.
    const std::vector<BusServiceRow> loop125 =
        serviceRows(stop52109Dd.rows, "125");
    ok &= drawEach(out, "s125", "Opp St. Michael's", loop125,
                   stop52109Dd.updatedAtEpoch, 0, false);

    ok &= drawEach(out, "wifi", "Opp St. Michael's", loop125,
                   stop52109Dd.updatedAtEpoch, 0, true);

    ok &= drawEach(out, "stale", "Opp St. Michael's", loop125,
                   stop52109Dd.updatedAtEpoch, 180000, false);

    ok &= drawEach(out, "term", "52109", term.rows, term.updatedAtEpoch, 0,
                   false);

    ok &= drawEach(out, "s136", "66271", serviceRows(stop66271.rows, "136"),
                   stop66271.updatedAtEpoch, 0, false);

    return ok ? 0 : 1;
}
