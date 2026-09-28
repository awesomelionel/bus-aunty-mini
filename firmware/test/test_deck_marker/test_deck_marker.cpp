#include <unity.h>

#include <cstdint>
#include <vector>

#include "board/board.h"
#include "core/arrival_parser.h"
#include "core/night_window.h"
#include "hal/power.h"
#include "host/host_gfx.h"
#include "ui/display.h"

namespace {

// 2024-01-01 00:00:00 UTC is 08:00 in Singapore, inside the day palette.
// 2024-01-01 12:00:00 UTC is 20:00, inside the night palette.
constexpr int64_t kDayEpoch = 1704067200;
constexpr int64_t kNightEpoch = 1704110400;

// The framed theme's double-decker icon: two 6x4 bars with one empty row
// between them. These are the pixels drawDoubleDeckMarker has always painted,
// just left of the countdown they belong to.
constexpr int kMarkerWidth = 6;
constexpr int kMarkerHeight = 9;
constexpr int kMarkerGapRow = 4;
constexpr int kMarkerPixels = kMarkerWidth * 8;

struct Point {
    int x;
    int y;
};

struct Marker {
    int x0;
    int y0;
    int x1;
    int y1;
    std::vector<Point> pixels;
};

BusServiceRow makeRow(BusType first, BusType second, BusType third, int64_t now) {
    BusServiceRow row;
    row.serviceNo = "125";
    row.label = "Sims";
    const BusType types[] = {first, second, third};
    for (int i = 0; i < 3; ++i) {
        row.arrivals[i].etaEpoch = now + (5 + i * 7) * 60;
        row.arrivals[i].load = BusLoad::SeatsAvailable;
        row.arrivals[i].type = types[i];
    }
    return row;
}

void draw(const char* boardName, bool win95, int64_t now, BusType first,
          BusType second, BusType third) {
    hostSetBoard(boardName);
    displaySetup();
    displaySetTheme(win95);
    const BusServiceRow row = makeRow(first, second, third, now);
    hal::PowerStatus power;
    power.percent = 80;
    displayShowArrivals("52109", {row}, now, 0, 1, 0, 1, power, 0, now, false);
}

std::vector<uint32_t> capture() {
    const int w = hostSpriteWidth();
    const int h = hostSpriteHeight();
    std::vector<uint32_t> pixels(static_cast<size_t>(w * h));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            pixels[static_cast<size_t>(y * w + x)] = hostPixel(x, y);
        }
    }
    return pixels;
}

std::vector<Point> diffPixels(const std::vector<uint32_t>& withMark,
                              const std::vector<uint32_t>& without) {
    std::vector<Point> out;
    const int w = hostSpriteWidth();
    const int h = hostSpriteHeight();
    TEST_ASSERT_EQUAL_INT(w * h, static_cast<int>(withMark.size()));
    TEST_ASSERT_EQUAL_INT(w * h, static_cast<int>(without.size()));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(y * w + x);
            if (withMark[i] != without[i]) {
                out.push_back({x, y});
            }
        }
    }
    return out;
}

// The two bars are one row apart, so a strict flood fill would split them.
// Points within one pixel of a cluster belong to the same icon.
std::vector<Marker> cluster(const std::vector<Point>& points) {
    std::vector<Marker> marks;
    std::vector<bool> used(points.size(), false);
    for (size_t i = 0; i < points.size(); ++i) {
        if (used[i]) {
            continue;
        }
        Marker mark;
        mark.x0 = mark.x1 = points[i].x;
        mark.y0 = mark.y1 = points[i].y;
        std::vector<size_t> stack = {i};
        used[i] = true;
        while (!stack.empty()) {
            const Point cur = points[stack.back()];
            stack.pop_back();
            mark.pixels.push_back(cur);
            if (cur.x < mark.x0) mark.x0 = cur.x;
            if (cur.x > mark.x1) mark.x1 = cur.x;
            if (cur.y < mark.y0) mark.y0 = cur.y;
            if (cur.y > mark.y1) mark.y1 = cur.y;
            for (size_t j = 0; j < points.size(); ++j) {
                if (used[j]) {
                    continue;
                }
                int dx = points[j].x - cur.x;
                int dy = points[j].y - cur.y;
                if (dx < 0) {
                    dx = -dx;
                }
                if (dy < 0) {
                    dy = -dy;
                }
                if (dx <= 1 && dy <= 2) {
                    used[j] = true;
                    stack.push_back(j);
                }
            }
        }
        marks.push_back(mark);
    }
    return marks;
}

void assertIsDeckMarker(const Marker& mark, const std::vector<uint32_t>& plain,
                        int spriteW) {
    TEST_ASSERT_EQUAL_INT(kMarkerWidth, mark.x1 - mark.x0 + 1);
    TEST_ASSERT_EQUAL_INT(kMarkerHeight, mark.y1 - mark.y0 + 1);
    TEST_ASSERT_EQUAL_INT(kMarkerPixels, static_cast<int>(mark.pixels.size()));
    // Beside the countdown, not in the destination gutter on the left.
    TEST_ASSERT_TRUE(mark.x0 > spriteW / 2);

    int gapPixels = 0;
    int barPixels = 0;
    for (const Point& p : mark.pixels) {
        const int row = p.y - mark.y0;
        TEST_ASSERT_TRUE(p.x >= mark.x0 && p.x <= mark.x1);
        if (row == kMarkerGapRow) {
            ++gapPixels;
        } else {
            ++barPixels;
            TEST_ASSERT_TRUE(row >= 0 && row < kMarkerHeight);
        }
    }
    TEST_ASSERT_EQUAL_INT(0, gapPixels);
    TEST_ASSERT_EQUAL_INT(kMarkerPixels, barPixels);

    // The icon sits a few pixels left of the countdown it belongs to. Sample
    // the single-deck frame, which has the same digits and no icon.
    const uint32_t bg = plain[static_cast<size_t>(mark.y0 * spriteW + (mark.x0 - 2))];
    int nearest = spriteW;
    for (int x = mark.x1 + 1; x < spriteW; ++x) {
        for (int y = mark.y0; y <= mark.y1; ++y) {
            if (plain[static_cast<size_t>(y * spriteW + x)] != bg) {
                if (x < nearest) {
                    nearest = x;
                }
            }
        }
    }
    // The icon is placed against the countdown's layout box. Visible ink
    // starts later by the glyph's left side bearing: 6px for "5", 11px for
    // the "1" in "12". The next column is most of a column-pitch away.
    const int gap = nearest - mark.x1;
    TEST_ASSERT_TRUE(gap >= 4 && gap <= 14);
}

std::vector<Marker> marksFor(const char* boardName, bool win95, int64_t now,
                             BusType first, BusType second, BusType third,
                             std::vector<uint32_t>* singleDeckFrame) {
    draw(boardName, win95, now, first, second, third);
    const std::vector<uint32_t> withMark = capture();
    draw(boardName, win95, now, BusType::SingleDeck, BusType::SingleDeck,
         BusType::SingleDeck);
    const std::vector<uint32_t> without = capture();
    if (singleDeckFrame != nullptr) {
        *singleDeckFrame = without;
    }
    return cluster(diffPixels(withMark, without));
}

}  // namespace

void test_win95_day_draws_the_mark_beside_a_double_decker() {
    TEST_ASSERT_FALSE(isNightAt(kDayEpoch, kLocalUtcOffsetSeconds));
    std::vector<uint32_t> single;
    const std::vector<Marker> marks =
        marksFor("tdisplay", true, kDayEpoch, BusType::DoubleDeck,
                 BusType::SingleDeck, BusType::SingleDeck, &single);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(marks.size()));
    assertIsDeckMarker(marks[0], single, hostSpriteWidth());
}

void test_win95_night_draws_the_mark_beside_a_double_decker() {
    TEST_ASSERT_TRUE(isNightAt(kNightEpoch, kLocalUtcOffsetSeconds));
    std::vector<uint32_t> single;
    const std::vector<Marker> marks =
        marksFor("tdisplay", true, kNightEpoch, BusType::DoubleDeck,
                 BusType::SingleDeck, BusType::SingleDeck, &single);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(marks.size()));
    assertIsDeckMarker(marks[0], single, hostSpriteWidth());
}

void test_win95_mark_follows_the_second_countdown() {
    std::vector<uint32_t> single;
    const std::vector<Marker> marks =
        marksFor("tdisplay", true, kDayEpoch, BusType::SingleDeck,
                 BusType::DoubleDeck, BusType::SingleDeck, &single);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(marks.size()));
    assertIsDeckMarker(marks[0], single, hostSpriteWidth());

    std::vector<uint32_t> ignored;
    const std::vector<Marker> first =
        marksFor("tdisplay", true, kDayEpoch, BusType::DoubleDeck,
                 BusType::SingleDeck, BusType::SingleDeck, &ignored);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(first.size()));
    TEST_ASSERT_TRUE(marks[0].x0 > first[0].x1);
}

void test_win95_does_not_mark_a_hidden_third_arrival() {
    std::vector<uint32_t> single;
    const std::vector<Marker> marks =
        marksFor("tdisplay", true, kDayEpoch, BusType::DoubleDeck,
                 BusType::DoubleDeck, BusType::DoubleDeck, &single);
    TEST_ASSERT_EQUAL_INT(2, static_cast<int>(marks.size()));
    const Marker& left = marks[0].x0 < marks[1].x0 ? marks[0] : marks[1];
    const Marker& right = marks[0].x0 < marks[1].x0 ? marks[1] : marks[0];
    assertIsDeckMarker(left, single, hostSpriteWidth());
    assertIsDeckMarker(right, single, hostSpriteWidth());
    TEST_ASSERT_TRUE(right.x0 > left.x1);
}

void test_plain_themes_draw_no_deck_marker() {
    const char* boards[] = {"sticks3", "feather", "tdisplay"};
    for (const char* boardName : boards) {
        const std::vector<Marker> marks =
            marksFor(boardName, false, kDayEpoch, BusType::DoubleDeck,
                     BusType::DoubleDeck, BusType::DoubleDeck, nullptr);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, static_cast<int>(marks.size()), boardName);
    }
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_win95_day_draws_the_mark_beside_a_double_decker);
    RUN_TEST(test_win95_night_draws_the_mark_beside_a_double_decker);
    RUN_TEST(test_win95_mark_follows_the_second_countdown);
    RUN_TEST(test_win95_does_not_mark_a_hidden_third_arrival);
    RUN_TEST(test_plain_themes_draw_no_deck_marker);
    return UNITY_END();
}
