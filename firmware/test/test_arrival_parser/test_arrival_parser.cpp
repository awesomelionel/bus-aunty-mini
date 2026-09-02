#include <unity.h>

#include "arrival_parser.h"

static const char* kSampleResponse = R"JSON({
  "busStops": [
    {
      "BusStopCode": "67379",
      "Services": [
        {
          "ServiceNo": "123",
          "NextBus": {"EstimatedArrival": "2024-03-20T12:34:56Z", "Load": "SEA", "Feature": "WAB", "Type": "SD"},
          "NextBus2": {"EstimatedArrival": "2024-03-20T12:44:56Z", "Load": "SDA", "Feature": "WAB", "Type": "SD"},
          "NextBus3": {"EstimatedArrival": "2024-03-20T12:54:56Z", "Load": "LSD", "Feature": "WAB", "Type": "SD"}
        }
      ],
      "UpdatedAt": "2024-03-20T12:34:56Z"
    }
  ]
})JSON";

void test_parses_service_and_three_etas() {
    ParsedBusStop result = parseBusArrivalResponse(kSampleResponse, "67379");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_STRING("67379", result.busStopCode.c_str());
    TEST_ASSERT_EQUAL(1, result.services.size());
    TEST_ASSERT_EQUAL_STRING("123", result.services[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_INT64(1710938096LL, result.services[0].times.eta1Epoch);
    TEST_ASSERT_EQUAL_INT64(1710938696LL, result.services[0].times.eta2Epoch);
    TEST_ASSERT_EQUAL_INT64(1710939296LL, result.services[0].times.eta3Epoch);
}

void test_missing_nextbus2_and_3_return_negative_one() {
    const char* json = R"JSON({
      "busStops": [
        {
          "BusStopCode": "67379",
          "Services": [
            { "ServiceNo": "5", "NextBus": {"EstimatedArrival": "2024-03-20T12:34:56Z"} }
          ]
        }
      ]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "67379");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT64(-1, result.services[0].times.eta2Epoch);
    TEST_ASSERT_EQUAL_INT64(-1, result.services[0].times.eta3Epoch);
}

void test_empty_bus_stops_is_invalid() {
    const char* json = R"JSON({"busStops": []})JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "67379");
    TEST_ASSERT_FALSE(result.valid);
}

void test_malformed_json_is_invalid() {
    ParsedBusStop result = parseBusArrivalResponse("not json", "67379");
    TEST_ASSERT_FALSE(result.valid);
}

void test_accepts_numeric_bus_stop_code() {
    const char* json = R"JSON({
      "busStops": [
        {
          "BusStopCode": 53389,
          "Services": [
            { "ServiceNo": "13", "NextBus": {"EstimatedArrival": "2026-09-02T22:18:31+08:00"} }
          ]
        }
      ]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "53389");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_STRING("53389", result.busStopCode.c_str());
}

void test_select_display_services_caps_at_six() {
    std::vector<BusService> services;
    for (int i = 0; i < 9; ++i) {
        BusService svc;
        svc.serviceNo = std::to_string(i);
        services.push_back(svc);
    }
    std::vector<BusService> selected = selectDisplayServices(services, 6);
    TEST_ASSERT_EQUAL(6, selected.size());
    TEST_ASSERT_EQUAL_STRING("0", selected[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("5", selected[5].serviceNo.c_str());
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_service_and_three_etas);
    RUN_TEST(test_missing_nextbus2_and_3_return_negative_one);
    RUN_TEST(test_empty_bus_stops_is_invalid);
    RUN_TEST(test_malformed_json_is_invalid);
    RUN_TEST(test_accepts_numeric_bus_stop_code);
    RUN_TEST(test_select_display_services_caps_at_six);
    return UNITY_END();
}
