#include <unity.h>

#include "core/html_escape.h"

void test_escapes_markup_and_quotes() {
    TEST_ASSERT_EQUAL_STRING(
        "Lionel&#39;s &lt;Home&gt; &amp; &quot;Office&quot;",
        escapeHtml("Lionel's <Home> & \"Office\"").c_str());
}

void test_leaves_plain_and_utf8_alone() {
    const std::string iphone = "Lionel\xe2\x80\x99s iPhone";
    TEST_ASSERT_EQUAL_STRING(iphone.c_str(), escapeHtml(iphone).c_str());
    TEST_ASSERT_EQUAL_STRING("", escapeHtml("").c_str());
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_escapes_markup_and_quotes);
    RUN_TEST(test_leaves_plain_and_utf8_alone);
    return UNITY_END();
}
