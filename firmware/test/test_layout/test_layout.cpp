#include <unity.h>

#include "core/layout.h"

namespace {

// The StickS3's panel, the 18px DejaVu row height the arrivals screen uses,
// and the width the battery icon reserves at the top right.
constexpr int kStickWidth = 240;
constexpr int kStickHeight = 135;
constexpr int kStickRowHeight = 18;
constexpr int kBatteryReservedWidth = 28;

}  // namespace

// The compact row keeps the service column and the header, and keeps the
// rightmost two of the old three ETA columns {119, 178, 236}. The dropped
// column is the width the destination now uses.
void test_stick_s3_compact_row_layout() {
    ArrivalsLayout layout = computeArrivalsLayout(kStickWidth, kStickHeight,
                                                   kStickRowHeight, kBatteryReservedWidth, false);

    TEST_ASSERT_EQUAL_size_t(6, layout.servicesPerScreen);
    TEST_ASSERT_EQUAL_INT(4, layout.serviceColX);
    TEST_ASSERT_EQUAL_INT(178, layout.etaColRightX[0]);
    TEST_ASSERT_EQUAL_INT(236, layout.etaColRightX[1]);
    TEST_ASSERT_EQUAL_INT(106, layout.headerCenterX);
    TEST_ASSERT_EQUAL_INT(131, layout.pageDotsY);
    TEST_ASSERT_FALSE(layout.hasLabels);
}

void test_row_height_is_passed_through() {
    ArrivalsLayout plain = computeArrivalsLayout(kStickWidth, kStickHeight,
                                                  kStickRowHeight, kBatteryReservedWidth, false);
    ArrivalsLayout labelled = computeArrivalsLayout(kStickWidth, kStickHeight,
                                                     kStickRowHeight, kBatteryReservedWidth, true);
    TEST_ASSERT_EQUAL_INT(kStickRowHeight, plain.rowHeight);
    TEST_ASSERT_EQUAL_INT(kStickRowHeight, labelled.rowHeight);
    TEST_ASSERT_EQUAL_size_t(plain.servicesPerScreen, labelled.servicesPerScreen);
}

// Single-line mode: 240x135 at DejaVu18 (h=18) fits 6 rows
void test_stick_s3_single_line_mode() {
    ArrivalsLayout layout = computeArrivalsLayout(kStickWidth, kStickHeight,
                                                   kStickRowHeight, kBatteryReservedWidth, false);
    TEST_ASSERT_EQUAL_size_t(6, layout.servicesPerScreen);
    TEST_ASSERT_EQUAL_INT(18, layout.rowHeight);
    TEST_ASSERT_FALSE(layout.hasLabels);
}

// Labels share the service line, so 240x135 still fits the single-line 6.
void test_stick_s3_labels_do_not_add_a_line() {
    ArrivalsLayout layout = computeArrivalsLayout(kStickWidth, kStickHeight,
                                                   kStickRowHeight, kBatteryReservedWidth, true);
    TEST_ASSERT_EQUAL_size_t(6, layout.servicesPerScreen);
    TEST_ASSERT_EQUAL_INT(18, layout.rowHeight);
    TEST_ASSERT_TRUE(layout.hasLabels);
}

// A different geometry has to produce a different answer, or something is
// still pinned to 240x135.
void test_a_taller_screen_fits_more_rows() {
    ArrivalsLayout layout =
        computeArrivalsLayout(320, 240, 18, kBatteryReservedWidth, false);

    TEST_ASSERT_EQUAL_size_t(12, layout.servicesPerScreen);
    TEST_ASSERT_EQUAL_INT(236, layout.pageDotsY);
    // Still right-aligned to the wider panel's edge, not the old one's.
    TEST_ASSERT_EQUAL_INT(316, layout.etaColRightX[kShownArrivals - 1]);
}

// The T-Display-S3's 320x170 panel at DejaVu24's row advance, which is 25 and
// not 24. Its taller screen buys bigger text rather than more rows: five fit
// here against the 240x135 boards' six at DejaVu18. Pinned so that a change
// to either the font choice or the layout arithmetic has to be deliberate.
void test_t_display_s3_layout_at_dejavu24_single_line() {
    ArrivalsLayout layout =
        computeArrivalsLayout(320, 170, 25, kBatteryReservedWidth, false);

    TEST_ASSERT_EQUAL_size_t(5, layout.servicesPerScreen);
    TEST_ASSERT_EQUAL_INT(25, layout.rowHeight);
    TEST_ASSERT_EQUAL_INT(4, layout.serviceColX);
    TEST_ASSERT_EQUAL_INT(238, layout.etaColRightX[0]);
    TEST_ASSERT_EQUAL_INT(316, layout.etaColRightX[1]);
    TEST_ASSERT_EQUAL_INT(146, layout.headerCenterX);
    TEST_ASSERT_EQUAL_INT(166, layout.pageDotsY);
    TEST_ASSERT_FALSE(layout.hasLabels);
}

// Labels share the service line, so the 25px row stays and five services fit.
void test_t_display_s3_labels_do_not_add_a_line() {
    ArrivalsLayout layout =
        computeArrivalsLayout(320, 170, 25, kBatteryReservedWidth, true);

    TEST_ASSERT_EQUAL_size_t(5, layout.servicesPerScreen);
    TEST_ASSERT_EQUAL_INT(25, layout.rowHeight);
    TEST_ASSERT_TRUE(layout.hasLabels);
}

void test_eta_columns_stay_ordered_and_on_the_margin() {
    ArrivalsLayout layout = computeArrivalsLayout(kStickWidth, kStickHeight,
                                                   kStickRowHeight, kBatteryReservedWidth, false);

    int gap = layout.etaColRightX[1] - layout.etaColRightX[0];
    TEST_ASSERT_TRUE(gap > 0);
    // The rightmost column still ends on the panel's right margin.
    TEST_ASSERT_EQUAL_INT(kStickWidth - 4, layout.etaColRightX[1]);
}

// The header must not run under the battery icon, and a board that reserves
// nothing for one gets a truly centred header.
void test_header_is_centred_clear_of_the_battery() {
    ArrivalsLayout layoutWithBattery = computeArrivalsLayout(
        kStickWidth, kStickHeight, kStickRowHeight, kBatteryReservedWidth, false);
    TEST_ASSERT_TRUE(layoutWithBattery.headerCenterX < kStickWidth / 2);
    
    ArrivalsLayout layoutNoBattery = computeArrivalsLayout(
        kStickWidth, kStickHeight, kStickRowHeight, 0, false);
    TEST_ASSERT_EQUAL_INT(kStickWidth / 2, layoutNoBattery.headerCenterX);
}

void test_a_row_taller_than_the_screen_fits_nothing() {
    ArrivalsLayout layout = computeArrivalsLayout(kStickWidth, kStickHeight,
                                                  200, kBatteryReservedWidth, false);
    TEST_ASSERT_EQUAL_size_t(0, layout.servicesPerScreen);
}

// Exactly one row's worth of height is all header and no services, and must
// not underflow into a huge size_t.
void test_a_screen_one_row_tall_fits_nothing() {
    TEST_ASSERT_EQUAL_size_t(
        0, computeArrivalsLayout(kStickWidth, 18, 18, kBatteryReservedWidth, false)
               .servicesPerScreen);
}

void test_a_zero_row_height_does_not_divide_by_zero() {
    TEST_ASSERT_EQUAL_size_t(
        0, computeArrivalsLayout(kStickWidth, kStickHeight, 0,
                                 kBatteryReservedWidth, false)
               .servicesPerScreen);
}

void test_win95_compact_rows_fit_four_on_t_display() {
    // T-Display-S3 Win95: 320x170, DejaVu24 advance 25. Chrome is 54px
    // (title 18 + column header 16 + status 16 + the two list margins).
    // The layout is handed listHeight + rowHeight because it reserves its
    // first row for a header the framed screen draws as chrome.
    constexpr int kWin95Width = 320;
    constexpr int kWin95Height = 170;
    constexpr int kWin95RowHeight = 25;
    constexpr int kWin95ChromeHeight = 54;
    constexpr int kWin95ListHeight = kWin95Height - kWin95ChromeHeight;

    ArrivalsLayout layout = computeArrivalsLayout(
        kWin95Width, kWin95ListHeight + kWin95RowHeight, kWin95RowHeight,
        0, true);

    TEST_ASSERT_EQUAL_size_t(4, layout.servicesPerScreen);
    TEST_ASSERT_EQUAL_INT(kWin95RowHeight, layout.rowHeight);
    TEST_ASSERT_EQUAL_INT(238, layout.etaColRightX[0]);
    TEST_ASSERT_EQUAL_INT(316, layout.etaColRightX[1]);
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_stick_s3_compact_row_layout);
    RUN_TEST(test_row_height_is_passed_through);
    RUN_TEST(test_stick_s3_single_line_mode);
    RUN_TEST(test_stick_s3_labels_do_not_add_a_line);
    RUN_TEST(test_a_taller_screen_fits_more_rows);
    RUN_TEST(test_t_display_s3_layout_at_dejavu24_single_line);
    RUN_TEST(test_t_display_s3_labels_do_not_add_a_line);
    RUN_TEST(test_eta_columns_stay_ordered_and_on_the_margin);
    RUN_TEST(test_header_is_centred_clear_of_the_battery);
    RUN_TEST(test_a_row_taller_than_the_screen_fits_nothing);
    RUN_TEST(test_a_screen_one_row_tall_fits_nothing);
    RUN_TEST(test_a_zero_row_height_does_not_divide_by_zero);
    RUN_TEST(test_win95_compact_rows_fit_four_on_t_display);
    return UNITY_END();
}
