#include <unity.h>

#include "core/wifi_credentials.h"

void test_ssid_rejects_empty_and_too_long() {
    TEST_ASSERT_FALSE(validateWifiSsid(""));
    TEST_ASSERT_TRUE(validateWifiSsid("a"));
    TEST_ASSERT_TRUE(validateWifiSsid(std::string(32, 'x')));
    TEST_ASSERT_FALSE(validateWifiSsid(std::string(33, 'x')));
}

void test_ssid_rejects_record_and_field_separators() {
    TEST_ASSERT_FALSE(validateWifiSsid(std::string("ab") + '\x1f' + "c"));
    TEST_ASSERT_FALSE(validateWifiSsid(std::string("ab") + '\x1e' + "c"));
}

void test_ssid_keeps_utf8_verbatim() {
    const std::string iphone = "Lionel\xe2\x80\x99s iPhone";
    TEST_ASSERT_TRUE(validateWifiSsid(iphone));
}

void test_password_allows_open_and_wpa_lengths() {
    TEST_ASSERT_TRUE(validateWifiPassword(""));
    TEST_ASSERT_FALSE(validateWifiPassword("short"));
    TEST_ASSERT_TRUE(validateWifiPassword("12345678"));
    TEST_ASSERT_TRUE(validateWifiPassword(std::string(63, 'p')));
    TEST_ASSERT_FALSE(validateWifiPassword(std::string(64, 'p')));
}

void test_password_rejects_separators() {
    TEST_ASSERT_FALSE(validateWifiPassword(std::string("password") + '\x1f'));
}

void test_build_drops_invalid_and_caps_at_five() {
    std::vector<WifiNetwork> rows = {
        {"Home", "abcdefgh"},
        {"", "abcdefgh"},
        {"Office", "abcdefgh"},
        {std::string("bad") + '\x1f', "abcdefgh"},
        {"Cafe", "short"},
        {"A", "abcdefgh"},
        {"B", "abcdefgh"},
        {"C", "abcdefgh"},
        {"D", "abcdefgh"},
    };
    std::vector<WifiNetwork> built = buildWifiNetworkList(rows);
    TEST_ASSERT_EQUAL_UINT32(5, built.size());
    TEST_ASSERT_EQUAL_STRING("Home", built[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Office", built[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("A", built[2].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("B", built[3].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("C", built[4].ssid.c_str());
}

void test_merge_keeps_existing_password_when_incoming_blank() {
    std::vector<WifiNetwork> existing = {{"Home", "secret12"},
                                         {"Office", "office99"}};
    std::vector<WifiNetwork> incoming = {{"Home", ""}, {"Hotspot", ""}};
    std::vector<WifiNetwork> merged = mergeWifiPasswords(existing, incoming);
    TEST_ASSERT_EQUAL_UINT32(2, merged.size());
    TEST_ASSERT_EQUAL_STRING("secret12", merged[0].password.c_str());
    TEST_ASSERT_EQUAL_STRING("", merged[1].password.c_str());
}

void test_round_trips_utf8_ssid() {
    const std::string iphone = "Lionel\xe2\x80\x99s iPhone";
    std::vector<WifiNetwork> nets = {{iphone, "hotspot1"}, {"Home", ""}};
    std::vector<WifiNetwork> back =
        deserializeWifiNetworks(serializeWifiNetworks(nets));
    TEST_ASSERT_EQUAL_UINT32(2, back.size());
    TEST_ASSERT_EQUAL_STRING(iphone.c_str(), back[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("hotspot1", back[0].password.c_str());
    TEST_ASSERT_EQUAL_STRING("Home", back[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", back[1].password.c_str());
}

void test_empty_blob_yields_no_networks() {
    TEST_ASSERT_EQUAL_UINT32(0, deserializeWifiNetworks("").size());
}

void test_corrupt_blob_yields_no_bad_networks() {
    TEST_ASSERT_EQUAL_UINT32(0, deserializeWifiNetworks("no-separator").size());
}

void test_ap_password_is_last_four_octets_hex() {
    const uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    TEST_ASSERT_EQUAL_STRING("CCDDEEFF", deriveApPassword(mac).c_str());
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_ssid_rejects_empty_and_too_long);
    RUN_TEST(test_ssid_rejects_record_and_field_separators);
    RUN_TEST(test_ssid_keeps_utf8_verbatim);
    RUN_TEST(test_password_allows_open_and_wpa_lengths);
    RUN_TEST(test_password_rejects_separators);
    RUN_TEST(test_build_drops_invalid_and_caps_at_five);
    RUN_TEST(test_merge_keeps_existing_password_when_incoming_blank);
    RUN_TEST(test_round_trips_utf8_ssid);
    RUN_TEST(test_empty_blob_yields_no_networks);
    RUN_TEST(test_corrupt_blob_yields_no_bad_networks);
    RUN_TEST(test_ap_password_is_last_four_octets_hex);
    return UNITY_END();
}
