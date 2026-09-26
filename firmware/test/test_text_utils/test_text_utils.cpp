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
    std::string result = truncateText("Caf\xC3\xA9", 60, mockWidth);
    TEST_ASSERT_EQUAL_STRING("Caf??", result.c_str());
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
    return UNITY_END();
}
