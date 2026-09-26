#include <unity.h>
#include "core/text_utils.h"

// Mock width function: 10px per character
int mockWidth(const char* s) {
    int width = 0;
    while (*s) {
        width += 10;
        ++s;
    }
    return width;
}

void test_returns_text_unchanged_when_it_fits() {
    std::string result = truncateText("Hello", 60, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Hello", result.c_str());
}

void test_truncates_at_word_boundary() {
    std::string result = truncateText("Hello World", 70, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Hello.", result.c_str());
}

void test_hard_cuts_long_single_word() {
    std::string result = truncateText("Supercalifragilistic", 100, mockWidth);
    // "Supercali." = 10 chars * 10px = 100px, fits exactly
    TEST_ASSERT_EQUAL_STRING("Supercali.", result.c_str());
}

void test_replaces_non_ascii_with_question_mark() {
    // "é" is 0xC3 0xA9 in UTF-8, should become one '?'
    std::string result = truncateText("Caf\xC3\xA9", 60, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Caf?", result.c_str());
}

void test_returns_dot_when_nothing_fits() {
    std::string result = truncateText("Hello", 5, mockWidth);
    TEST_ASSERT_EQUAL_STRING(".", result.c_str());
}

void test_empty_string_returns_empty() {
    std::string result = truncateText("", 50, mockWidth);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_multiple_word_boundaries() {
    std::string result = truncateText("The quick brown fox", 120, mockWidth);
    // "The quick b." = 11 chars = 110px, fits in 120px
    TEST_ASSERT_EQUAL_STRING("The quick.", result.c_str());
}

void test_appends_dot_when_truncated_at_exact_width() {
    // "Hello." = 60px, should fit exactly
    std::string result = truncateText("Hello World", 60, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Hello.", result.c_str());
}

void test_cut_after_st_no_double_dot() {
    // "St." should become "St." not "St.."
    std::string result = truncateText("St. Michael's Ter", 40, mockWidth);
    TEST_ASSERT_EQUAL_STRING("St.", result.c_str());
}

void test_exact_fit_without_truncation() {
    // "Hello" = 50px, fits exactly in 50px
    std::string result = truncateText("Hello", 50, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Hello", result.c_str());
}

void test_fit_plus_one_pixel() {
    // "Hello" = 50px, fits in 51px without truncation
    std::string result = truncateText("Hello", 51, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Hello", result.c_str());
}

void test_27_char_label_with_marker_width_reserved() {
    // Real 27-char label: "Bef Ang Mo Kio Depot Exit" (27 chars exactly)
    // " 2nd" marker = 4 chars = 40px
    // maxLabelWidth = 280px, reserved 40px for marker = 240px available
    // 24 chars fit: "Bef Ang Mo Kio Depot Ex." = 24 chars = 240px
    std::string label = "Bef Ang Mo Kio Depot Exit A";  // 27 chars
    TEST_ASSERT_EQUAL(27, label.size());
    std::string result = truncateText(label, 240, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Bef Ang Mo Kio Depot.", result.c_str());
}

void test_one_pixel_too_wide() {
    // "Hello" = 50px, doesn't fit in 49px
    // Should truncate to "Hell." = 50px
    // But that's too wide, so "Hel." = 40px
    std::string result = truncateText("Hello", 49, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Hell.", result.c_str());
}

void test_final_append_dot_exceeds_width() {
    // "Hello" = 50px, fits in 55px without needing "."
    std::string result = truncateText("Hello", 55, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Hello", result.c_str());
    
    // But "HelloWorld" = 100px, doesn't fit in 55px
    // Should hard-cut to "Hell." = 50px
    std::string result2 = truncateText("HelloWorld", 55, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Hell.", result2.c_str());
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_returns_text_unchanged_when_it_fits);
    RUN_TEST(test_truncates_at_word_boundary);
    RUN_TEST(test_hard_cuts_long_single_word);
    RUN_TEST(test_replaces_non_ascii_with_question_mark);
    RUN_TEST(test_returns_dot_when_nothing_fits);
    RUN_TEST(test_empty_string_returns_empty);
    RUN_TEST(test_multiple_word_boundaries);
    RUN_TEST(test_appends_dot_when_truncated_at_exact_width);
    RUN_TEST(test_cut_after_st_no_double_dot);
    RUN_TEST(test_exact_fit_without_truncation);
    RUN_TEST(test_fit_plus_one_pixel);
    RUN_TEST(test_27_char_label_with_marker_width_reserved);
    RUN_TEST(test_one_pixel_too_wide);
    RUN_TEST(test_final_append_dot_exceeds_width);
    return UNITY_END();
}
