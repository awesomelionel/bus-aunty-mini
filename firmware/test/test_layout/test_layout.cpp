#include <unity.h>

#include "core/layout.h"

namespace {

// The StickS3's panel, the 18px DejaVu row height the arrivals screen uses,
// and the width the battery icon reserves at the top right.
constexpr int kStickWidth = 240;
constexpr int kStickHeight = 135;
constexpr int kStickRowHeight = 18;
constexpr int kBatteryReservedWidth = 28;

ArrivalsLayout stickS3Layout() {
    return computeArrivalsLayout(kStickWidth, kStickHeight, kStickRowHeight,
                                 kBatteryReservedWidth);
}

}  // namespace

// The refactor that introduced this module moved these numbers out of
// ui/display.cpp, where they were hardcoded. Reproducing them exactly is what
// makes that move provably a no-op on the StickS3.
void test_reproduces_the_original_stick_s3_layout() {
    ArrivalsLayout layout = stickS3Layout();

    TEST_ASSERT_EQUAL_size_t(6, layout.servicesPerScreen);
    TEST_ASSERT_EQUAL_INT(4, layout.serviceColX);
    TEST_ASSERT_EQUAL_INT(119, layout.etaColRightX[0]);
    TEST_ASSERT_EQUAL_INT(178, layout.etaColRightX[1]);
    TEST_ASSERT_EQUAL_INT(236, layout.etaColRightX[2]);
    TEST_ASSERT_EQUAL_INT(106, layout.headerCenterX);
    TEST_ASSERT_EQUAL_INT(131, layout.pageDotsY);
}

void test_row_height_is_passed_through() {
    TEST_ASSERT_EQUAL_INT(kStickRowHeight, stickS3Layout().rowHeight);
}

// A different geometry has to produce a different answer, or something is
// still pinned to 240x135.
void test_a_taller_screen_fits_more_rows() {
    ArrivalsLayout layout =
        computeArrivalsLayout(320, 240, 18, kBatteryReservedWidth);

    TEST_ASSERT_EQUAL_size_t(12, layout.servicesPerScreen);
    TEST_ASSERT_EQUAL_INT(236, layout.pageDotsY);
    // Still right-aligned to the wider panel's edge, not the old one's.
    TEST_ASSERT_EQUAL_INT(316, layout.etaColRightX[kArrivalsPerService - 1]);
}

void test_eta_columns_are_ordered_and_evenly_spread() {
    ArrivalsLayout layout = stickS3Layout();

    int firstGap = layout.etaColRightX[1] - layout.etaColRightX[0];
    int secondGap = layout.etaColRightX[2] - layout.etaColRightX[1];
    TEST_ASSERT_TRUE(firstGap > 0 && secondGap > 0);
    // Equal pitch up to the one pixel that rounding a fractional pitch costs.
    TEST_ASSERT_INT_WITHIN(1, firstGap, secondGap);
}

// The header must not run under the battery icon, and a board that reserves
// nothing for one gets a truly centred header.
void test_header_is_centred_clear_of_the_battery() {
    TEST_ASSERT_TRUE(stickS3Layout().headerCenterX < kStickWidth / 2);
    TEST_ASSERT_EQUAL_INT(
        kStickWidth / 2,
        computeArrivalsLayout(kStickWidth, kStickHeight, kStickRowHeight, 0)
            .headerCenterX);
}

void test_a_row_taller_than_the_screen_fits_nothing() {
    ArrivalsLayout layout = computeArrivalsLayout(kStickWidth, kStickHeight,
                                                  200, kBatteryReservedWidth);
    TEST_ASSERT_EQUAL_size_t(0, layout.servicesPerScreen);
}

// Exactly one row's worth of height is all header and no services, and must
// not underflow into a huge size_t.
void test_a_screen_one_row_tall_fits_nothing() {
    TEST_ASSERT_EQUAL_size_t(
        0, computeArrivalsLayout(kStickWidth, 18, 18, kBatteryReservedWidth)
               .servicesPerScreen);
}

void test_a_zero_row_height_does_not_divide_by_zero() {
    TEST_ASSERT_EQUAL_size_t(
        0, computeArrivalsLayout(kStickWidth, kStickHeight, 0,
                                 kBatteryReservedWidth)
               .servicesPerScreen);
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_reproduces_the_original_stick_s3_layout);
    RUN_TEST(test_row_height_is_passed_through);
    RUN_TEST(test_a_taller_screen_fits_more_rows);
    RUN_TEST(test_eta_columns_are_ordered_and_evenly_spread);
    RUN_TEST(test_header_is_centred_clear_of_the_battery);
    RUN_TEST(test_a_row_taller_than_the_screen_fits_nothing);
    RUN_TEST(test_a_screen_one_row_tall_fits_nothing);
    RUN_TEST(test_a_zero_row_height_does_not_divide_by_zero);
    return UNITY_END();
}
