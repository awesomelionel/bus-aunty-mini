#include <unity.h>

#include "core/bus_stop_config.h"

void test_label_falls_back_to_code_when_unnamed() {
    BusStopConfig stop{"53389", ""};
    TEST_ASSERT_EQUAL_STRING("53389", busStopLabel(stop).c_str());
}

void test_label_prefers_name_when_given() {
    BusStopConfig stop{"53389", "Home"};
    TEST_ASSERT_EQUAL_STRING("Home", busStopLabel(stop).c_str());
}

void test_code_keeps_leading_zeros() {
    std::string out;
    TEST_ASSERT_TRUE(normalizeBusStopCode("00481", &out));
    TEST_ASSERT_EQUAL_STRING("00481", out.c_str());
}

void test_code_accepts_three_to_five_digits() {
    std::string out;
    TEST_ASSERT_TRUE(normalizeBusStopCode("481", &out));
    TEST_ASSERT_TRUE(normalizeBusStopCode("01012", &out));
    TEST_ASSERT_FALSE(normalizeBusStopCode("48", &out));
    TEST_ASSERT_FALSE(normalizeBusStopCode("123456", &out));
}

void test_code_is_trimmed_before_validating() {
    std::string out;
    TEST_ASSERT_TRUE(normalizeBusStopCode("  53389\n", &out));
    TEST_ASSERT_EQUAL_STRING("53389", out.c_str());
}

void test_code_rejects_non_digits_and_blanks() {
    std::string out;
    TEST_ASSERT_FALSE(normalizeBusStopCode("", &out));
    TEST_ASSERT_FALSE(normalizeBusStopCode("   ", &out));
    TEST_ASSERT_FALSE(normalizeBusStopCode("5338A", &out));
    TEST_ASSERT_FALSE(normalizeBusStopCode("53-89", &out));
}

void test_name_is_trimmed_and_capped() {
    TEST_ASSERT_EQUAL_STRING("Home", normalizeBusStopName("  Home  ").c_str());
    TEST_ASSERT_EQUAL_STRING(
        "Ang Mo Kio Aven",
        normalizeBusStopName("Ang Mo Kio Aven").c_str());
    TEST_ASSERT_TRUE(normalizeBusStopName("This name is far too long").size() <=
                     kBusStopNameMaxChars);
}

void test_name_strips_control_characters() {
    TEST_ASSERT_EQUAL_STRING("HomeStop",
                             normalizeBusStopName("Home\x1fStop").c_str());
}

void test_name_strips_characters_that_break_the_portal_form() {
    TEST_ASSERT_EQUAL_STRING("Ah Mas",
                             normalizeBusStopName("Ah Ma's").c_str());
    TEST_ASSERT_EQUAL_STRING("bHome",
                             normalizeBusStopName("<b>Home").c_str());
}

void test_build_drops_rows_without_a_valid_code() {
    std::vector<BusStopConfig> rows = {
        {"53389", "Home"}, {"", "Ignored"}, {"bad", "Ignored"}, {"67379", ""}};
    std::vector<BusStopConfig> stops = buildBusStopList(rows);
    TEST_ASSERT_EQUAL_UINT32(2, stops.size());
    TEST_ASSERT_EQUAL_STRING("53389", stops[0].code.c_str());
    TEST_ASSERT_EQUAL_STRING("Home", stops[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("67379", stops[1].code.c_str());
    TEST_ASSERT_EQUAL_STRING("67379", busStopLabel(stops[1]).c_str());
}

void test_build_caps_at_four_stops() {
    std::vector<BusStopConfig> rows = {{"11111", ""}, {"22222", ""},
                                       {"33333", ""}, {"44444", ""},
                                       {"55555", ""}};
    TEST_ASSERT_EQUAL_UINT32(kMaxBusStops, buildBusStopList(rows).size());
}

void test_round_trips_through_serialization() {
    std::vector<BusStopConfig> stops = {{"00481", "Home"},
                                        {"53389", ""},
                                        {"01012", "Opp Mall"}};
    std::vector<BusStopConfig> back =
        deserializeBusStops(serializeBusStops(stops));
    TEST_ASSERT_EQUAL_UINT32(3, back.size());
    for (size_t i = 0; i < back.size(); ++i) {
        TEST_ASSERT_EQUAL_STRING(stops[i].code.c_str(), back[i].code.c_str());
        TEST_ASSERT_EQUAL_STRING(stops[i].name.c_str(), back[i].name.c_str());
    }
}

void test_empty_blob_yields_no_stops() {
    TEST_ASSERT_EQUAL_UINT32(0, deserializeBusStops("").size());
}

void test_corrupt_blob_yields_no_bad_stops() {
    TEST_ASSERT_EQUAL_UINT32(0, deserializeBusStops("not-a-code").size());
    TEST_ASSERT_EQUAL_UINT32(1, deserializeBusStops("53389").size());
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_label_falls_back_to_code_when_unnamed);
    RUN_TEST(test_label_prefers_name_when_given);
    RUN_TEST(test_code_keeps_leading_zeros);
    RUN_TEST(test_code_accepts_three_to_five_digits);
    RUN_TEST(test_code_is_trimmed_before_validating);
    RUN_TEST(test_code_rejects_non_digits_and_blanks);
    RUN_TEST(test_name_is_trimmed_and_capped);
    RUN_TEST(test_name_strips_control_characters);
    RUN_TEST(test_name_strips_characters_that_break_the_portal_form);
    RUN_TEST(test_build_drops_rows_without_a_valid_code);
    RUN_TEST(test_build_caps_at_four_stops);
    RUN_TEST(test_round_trips_through_serialization);
    RUN_TEST(test_empty_blob_yields_no_stops);
    RUN_TEST(test_corrupt_blob_yields_no_bad_stops);
    return UNITY_END();
}
