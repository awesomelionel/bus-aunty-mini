#include <unity.h>

#include "core/wifi_policy.h"

namespace {

std::vector<WifiNetwork> three() {
    return {{"Home", "abcdefgh"},
            {"Office", "abcdefgh"},
            {"Hotspot", "abcdefgh"}};
}

}  // namespace

void test_candidates_follow_saved_priority_not_scan_order() {
    std::vector<std::string> seen = {"Hotspot", "Home"};
    std::vector<size_t> got = selectWifiCandidates(three(), seen);
    TEST_ASSERT_EQUAL_UINT32(2, got.size());
    TEST_ASSERT_EQUAL_UINT32(0, got[0]);
    TEST_ASSERT_EQUAL_UINT32(2, got[1]);
}

void test_candidates_ignore_unseen_and_unsaved() {
    std::vector<size_t> none = selectWifiCandidates(three(), {"Cafe"});
    TEST_ASSERT_EQUAL_UINT32(0, none.size());
    std::vector<size_t> onlyOffice =
        selectWifiCandidates(three(), {"Office", "Cafe"});
    TEST_ASSERT_EQUAL_UINT32(1, onlyOffice.size());
    TEST_ASSERT_EQUAL_UINT32(1, onlyOffice[0]);
}

void test_candidates_dedupe_seen() {
    std::vector<size_t> got = selectWifiCandidates(three(), {"Home", "Home"});
    TEST_ASSERT_EQUAL_UINT32(1, got.size());
    TEST_ASSERT_EQUAL_UINT32(0, got[0]);
}

void test_backoff_steps_then_caps() {
    TEST_ASSERT_EQUAL_UINT32(5000, wifiBackoffMs(0));
    TEST_ASSERT_EQUAL_UINT32(15000, wifiBackoffMs(1));
    TEST_ASSERT_EQUAL_UINT32(60000, wifiBackoffMs(2));
    TEST_ASSERT_EQUAL_UINT32(60000, wifiBackoffMs(99));
}

void test_upgrade_skips_when_already_on_top() {
    TEST_ASSERT_FALSE(shouldUpgradeWifi(three(), "Home", {"Home", "Office"}));
}

void test_upgrade_fires_when_better_is_seen() {
    TEST_ASSERT_TRUE(shouldUpgradeWifi(three(), "Hotspot", {"Home"}));
    TEST_ASSERT_TRUE(shouldUpgradeWifi(three(), "Office", {"Home", "Office"}));
    TEST_ASSERT_FALSE(
        shouldUpgradeWifi(three(), "Office", {"Office", "Hotspot"}));
}

void test_upgrade_treats_unknown_current_as_lowest() {
    TEST_ASSERT_TRUE(shouldUpgradeWifi(three(), "Cafe", {"Hotspot"}));
    TEST_ASSERT_FALSE(shouldUpgradeWifi(three(), "Cafe", {"Cafe"}));
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_candidates_follow_saved_priority_not_scan_order);
    RUN_TEST(test_candidates_ignore_unseen_and_unsaved);
    RUN_TEST(test_candidates_dedupe_seen);
    RUN_TEST(test_backoff_steps_then_caps);
    RUN_TEST(test_upgrade_skips_when_already_on_top);
    RUN_TEST(test_upgrade_fires_when_better_is_seen);
    RUN_TEST(test_upgrade_treats_unknown_current_as_lowest);
    return UNITY_END();
}
