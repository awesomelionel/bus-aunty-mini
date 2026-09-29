#include <unity.h>

#include "core/header_format.h"
#include "core/iso8601.h"

// Mock width function: 10px per character
int mockWidth(const char* s) {
    int width = 0;
    while (*s) {
        width += 10;
        ++s;
    }
    return width;
}

int64_t updatedAt() {
    return parseIso8601ToEpoch("2026-09-28T16:33:03+08:00");
}

void test_full_header_fits() {
    std::string result = buildHeader("Stop Name", 0, 2, 0, updatedAt(), 300, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Stop Name (1/2)", result.c_str());
}

void test_name_that_fits_is_still_made_ascii() {
    std::string result = buildHeader("S\xE2\x80\x99goon", 0, 1, 0,
                                     updatedAt(), 300, mockWidth);
    TEST_ASSERT_EQUAL_STRING("S'goon (1/1)", result.c_str());
}

void test_fresh_under_150s_has_no_age() {
    std::string result =
        buildHeader("Stop", 0, 1, 149000, updatedAt(), 400, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Stop (1/1)", result.c_str());
}

void test_stale_header_uses_as_of() {
    std::string result =
        buildHeader("Stop", 0, 1, 150000, updatedAt(), 400, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Stop (1/1) as of 16:33", result.c_str());
}

void test_drops_page_indicator_keeps_as_of() {
    // "Stop Name (1/2) as of 16:33" is 28 chars. 200px holds 20.
    // Without the page: "Stop Name as of 16:33" is 22 chars, still over.
    // Short form "Stop Name 16:33" is 15 chars and fits.
    std::string result =
        buildHeader("Stop Name", 0, 2, 180000, updatedAt(), 200, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Stop Name 16:33", result.c_str());
}

void test_long_form_kept_when_it_fits_without_page() {
    // "Stop Name (1/2) as of 16:33" = 28 chars = 280px, over 250.
    // "Stop Name as of 16:33" = 22 chars = 220px, fits.
    std::string result =
        buildHeader("Stop Name", 0, 2, 180000, updatedAt(), 250, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Stop Name as of 16:33", result.c_str());
}

void test_truncates_name_keeps_time() {
    std::string result = buildHeader("Very Long Stop Name", 0, 2, 180000,
                                     updatedAt(), 120, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Very. 16:33", result.c_str());
}

void test_extremely_narrow_width_keeps_time() {
    std::string result =
        buildHeader("Very Long Name", 0, 1, 180000, updatedAt(), 60, mockWidth);
    TEST_ASSERT_EQUAL_STRING(" 16:33", result.c_str());
}

void test_fresh_data_no_age() {
    std::string result = buildHeader("Stop", 0, 1, 0, -1, 200, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Stop (1/1)", result.c_str());
}

void test_updated_at_matches_iso_parse() {
    int64_t epoch = parseIso8601ToEpoch("2026-09-28T16:33:03+08:00");
    std::string result =
        buildHeader("S", 0, 1, 200000, epoch, 400, mockWidth);
    TEST_ASSERT_EQUAL_STRING("S (1/1) as of 16:33", result.c_str());
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_name_that_fits_is_still_made_ascii);
    RUN_TEST(test_full_header_fits);
    RUN_TEST(test_fresh_under_150s_has_no_age);
    RUN_TEST(test_stale_header_uses_as_of);
    RUN_TEST(test_drops_page_indicator_keeps_as_of);
    RUN_TEST(test_long_form_kept_when_it_fits_without_page);
    RUN_TEST(test_truncates_name_keeps_time);
    RUN_TEST(test_extremely_narrow_width_keeps_time);
    RUN_TEST(test_fresh_data_no_age);
    RUN_TEST(test_updated_at_matches_iso_parse);
    return UNITY_END();
}
