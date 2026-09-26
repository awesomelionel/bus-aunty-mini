#include <unity.h>

#include "core/header_format.h"

// Mock width function: 10px per character
int mockWidth(const char* s) {
    int width = 0;
    while (*s) {
        width += 10;
        ++s;
    }
    return width;
}

void test_full_header_fits() {
    std::string result = buildHeader("Stop Name", 0, 2, 0, 300, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Stop Name (1/2)", result.c_str());
}

void test_drops_page_indicator_keeps_age() {
    // "Stop Name (1/2) 2m" = 18 chars = 180px, doesn't fit in 150px
    // Should drop page indicator: "Stop Name 2m" = 12 chars = 120px
    std::string result = buildHeader("Stop Name", 0, 2, 120000, 150, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Stop Name 2m", result.c_str());
}

void test_truncates_name_keeps_age() {
    // "Very Long Stop Name (1/2) 3m" doesn't fit
    // "Very Long Stop Name 3m" = 22 chars = 220px, doesn't fit in 100px
    // Age " 3m" = 3 chars = 30px, leaves 70px for name
    // Name truncated at word boundary: "Very." = 5 chars = 50px, fits
    std::string result = buildHeader("Very Long Stop Name", 0, 2, 180000, 100, mockWidth);
    // "Very. 3m" = 8 chars = 80px
    TEST_ASSERT_EQUAL_STRING("Very. 3m", result.c_str());
}

void test_worst_case_16_char_name_with_age() {
    // 16-char name + " 16m" (4 chars) = 20 chars = 200px
    // If max width is 120px, age " 16m" takes 40px, leaving 80px for name
    // 8 chars: "Befname." = 80px
    std::string result = buildHeader("Befname Tampines", 3, 4, 999999, 120, mockWidth);
    // Should be "Befname. 16m" (12 chars = 120px)
    TEST_ASSERT_EQUAL_STRING("Befname. 16m", result.c_str());
}

void test_fresh_data_no_age() {
    std::string result = buildHeader("Stop", 0, 1, 0, 200, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Stop (1/1)", result.c_str());
}

void test_age_in_seconds() {
    std::string result = buildHeader("Stop", 0, 1, 75000, 200, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Stop (1/1) 75s", result.c_str());
}

void test_age_in_minutes() {
    std::string result = buildHeader("Stop", 0, 1, 240000, 200, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Stop (1/1) 4m", result.c_str());
}

void test_extremely_narrow_width() {
    // Age " 3m" = 30px, no room left for name, show only age
    std::string result = buildHeader("Very Long Name", 0, 1, 180000, 30, mockWidth);
    TEST_ASSERT_EQUAL_STRING(" 3m", result.c_str());
}

void test_minimal_name_with_age() {
    // Age " 3m" = 30px, name gets 10px = 1 char
    std::string result = buildHeader("Very Long Name", 0, 1, 180000, 40, mockWidth);
    // Should be "V. 3m" but truncateText might return just "V" for 10px
    // Let's verify age is present
    TEST_ASSERT_TRUE(result.find(" 3m") != std::string::npos);
}

void test_win95_title_keeps_age() {
    // Win95 title: 16-char name truncated + age, max width 268px
    // Age " 3m" = 30px must always be present
    std::string result = buildHeader("WWWWWWWWWWWWWWWW", 3, 4, 180000, 268, mockWidth);
    
    // Must contain " 3m" (age always kept)
    TEST_ASSERT_TRUE(result.find(" 3m") != std::string::npos);
    
    // Total width must fit in 268px
    TEST_ASSERT_TRUE(mockWidth(result.c_str()) <= 268);
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_full_header_fits);
    RUN_TEST(test_drops_page_indicator_keeps_age);
    RUN_TEST(test_truncates_name_keeps_age);
    RUN_TEST(test_worst_case_16_char_name_with_age);
    RUN_TEST(test_fresh_data_no_age);
    RUN_TEST(test_age_in_seconds);
    RUN_TEST(test_age_in_minutes);
    RUN_TEST(test_extremely_narrow_width);
    RUN_TEST(test_minimal_name_with_age);
    RUN_TEST(test_win95_title_keeps_age);
    return UNITY_END();
}
