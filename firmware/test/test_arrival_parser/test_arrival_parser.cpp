#include <unity.h>

#include "core/arrival_parser.h"

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
    TEST_ASSERT_EQUAL_INT64(1710938096LL, result.services[0].arrivals[0].etaEpoch);
    TEST_ASSERT_EQUAL_INT64(1710938696LL, result.services[0].arrivals[1].etaEpoch);
    TEST_ASSERT_EQUAL_INT64(1710939296LL, result.services[0].arrivals[2].etaEpoch);
}

void test_parses_load_for_each_arrival() {
    ParsedBusStop result = parseBusArrivalResponse(kSampleResponse, "67379");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(BusLoad::SeatsAvailable, result.services[0].arrivals[0].load);
    TEST_ASSERT_EQUAL(BusLoad::StandingAvailable,
                      result.services[0].arrivals[1].load);
    TEST_ASSERT_EQUAL(BusLoad::LimitedStanding,
                      result.services[0].arrivals[2].load);
}

// Type rides on each NextBus, not on the service, so a double decker in front
// of a single has to come back that way round.
void test_parses_vehicle_type_for_each_arrival() {
    const char* json = R"JSON({
      "busStops": [
        {
          "BusStopCode": "67379",
          "Services": [
            {
              "ServiceNo": "123",
              "NextBus": {"EstimatedArrival": "2024-03-20T12:34:56Z", "Type": "DD"},
              "NextBus2": {"EstimatedArrival": "2024-03-20T12:44:56Z", "Type": "SD"},
              "NextBus3": {"EstimatedArrival": "2024-03-20T12:54:56Z", "Type": "BD"}
            }
          ]
        }
      ]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "67379");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(BusType::DoubleDeck, result.services[0].arrivals[0].type);
    TEST_ASSERT_EQUAL(BusType::SingleDeck, result.services[0].arrivals[1].type);
    TEST_ASSERT_EQUAL(BusType::Bendy, result.services[0].arrivals[2].type);
}

void test_vehicle_type_codes_map_to_decks() {
    TEST_ASSERT_EQUAL(BusType::SingleDeck, parseBusType("SD"));
    TEST_ASSERT_EQUAL(BusType::DoubleDeck, parseBusType("DD"));
    TEST_ASSERT_EQUAL(BusType::Bendy, parseBusType("BD"));
}

// An unreadable type must not become a double decker: the marker is a promise
// about the bus turning up, and a wrong one is worse than none.
void test_unrecognised_vehicle_type_is_unknown() {
    TEST_ASSERT_EQUAL(BusType::Unknown, parseBusType(""));
    TEST_ASSERT_EQUAL(BusType::Unknown, parseBusType("dd"));
    TEST_ASSERT_EQUAL(BusType::Unknown, parseBusType("XYZ"));
}

void test_missing_vehicle_type_field_is_unknown() {
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
    TEST_ASSERT_EQUAL(BusType::Unknown, result.services[0].arrivals[0].type);
}

void test_load_codes_map_to_crowding_levels() {
    TEST_ASSERT_EQUAL(BusLoad::SeatsAvailable, parseBusLoad("SEA"));
    TEST_ASSERT_EQUAL(BusLoad::StandingAvailable, parseBusLoad("SDA"));
    TEST_ASSERT_EQUAL(BusLoad::LimitedStanding, parseBusLoad("LSD"));
}

void test_unrecognised_load_is_unknown() {
    TEST_ASSERT_EQUAL(BusLoad::Unknown, parseBusLoad(""));
    TEST_ASSERT_EQUAL(BusLoad::Unknown, parseBusLoad("sea"));
    TEST_ASSERT_EQUAL(BusLoad::Unknown, parseBusLoad("XYZ"));
}

void test_missing_load_field_is_unknown() {
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
    TEST_ASSERT_EQUAL(BusLoad::Unknown, result.services[0].arrivals[0].load);
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
    TEST_ASSERT_EQUAL_INT64(-1, result.services[0].arrivals[1].etaEpoch);
    TEST_ASSERT_EQUAL_INT64(-1, result.services[0].arrivals[2].etaEpoch);
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

void test_matches_numeric_bus_stop_code_with_leading_zero() {
    const char* json = R"JSON({
      "busStops": [
        {
          "BusStopCode": 1012,
          "Services": [
            { "ServiceNo": "10", "NextBus": {"EstimatedArrival": "2026-09-02T22:18:31+08:00"} }
          ]
        }
      ]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "01012");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_STRING("01012", result.busStopCode.c_str());
}

void test_select_display_services_caps_at_six() {
    std::vector<BusService> services;
    for (int i = 0; i < 9; ++i) {
        BusService svc;
        svc.serviceNo = std::to_string(i);
        services.push_back(svc);
    }
    std::vector<BusService> selected = selectServicePage(services, 6, 0);
    TEST_ASSERT_EQUAL(6, selected.size());
    TEST_ASSERT_EQUAL_STRING("0", selected[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("5", selected[5].serviceNo.c_str());
}

static std::vector<BusService> makeServices(size_t count) {
    std::vector<BusService> services;
    for (size_t i = 0; i < count; ++i) {
        BusService svc;
        svc.serviceNo = std::to_string(i);
        services.push_back(svc);
    }
    return services;
}

void test_page_count_rounds_up() {
    TEST_ASSERT_EQUAL_UINT32(0, servicePageCount(0, 6));
    TEST_ASSERT_EQUAL_UINT32(1, servicePageCount(1, 6));
    TEST_ASSERT_EQUAL_UINT32(1, servicePageCount(6, 6));
    TEST_ASSERT_EQUAL_UINT32(2, servicePageCount(7, 6));
    TEST_ASSERT_EQUAL_UINT32(3, servicePageCount(13, 6));
}

void test_second_page_continues_where_first_ended() {
    std::vector<BusService> services = makeServices(8);
    std::vector<BusService> page = selectServicePage(services, 6, 1);
    TEST_ASSERT_EQUAL_UINT32(2, page.size());
    TEST_ASSERT_EQUAL_STRING("6", page[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("7", page[1].serviceNo.c_str());
}

void test_page_past_the_end_is_empty() {
    std::vector<BusService> services = makeServices(8);
    TEST_ASSERT_EQUAL_UINT32(0, selectServicePage(services, 6, 2).size());
}

// V2 API tests

void test_v2_parses_string_bus_stop_code() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "124",
          "Loop": {"IsLoop": false, "LoopDesc": null},
          "NextBus": {"EstimatedArrival": "2026-09-26T16:41:44+08:00", "Load": "SEA", "Type": "SD", "Label": "To St. Michael's Ter", "VisitNumber": "1"}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_STRING("52109", result.busStopCode.c_str());
}

void test_v2_parses_label_and_visit_number() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "125",
          "Loop": {"IsLoop": true, "LoopDesc": "Sims Dr"},
          "NextBus": {"EstimatedArrival": "2026-09-26T16:47:10+08:00", "Label": "To Sims", "VisitNumber": "1"},
          "NextBus2": {"EstimatedArrival": "2026-09-26T16:57:09+08:00", "Label": "To St. Michael's Ter", "VisitNumber": "2"}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_STRING("1", result.services[0].arrivals[0].visitNumber.c_str());
    TEST_ASSERT_EQUAL_STRING("2", result.services[0].arrivals[1].visitNumber.c_str());
    TEST_ASSERT_TRUE(result.services[0].isLoop);
}

void test_v2_empty_label_is_parsed() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "186",
          "NextBus": {"EstimatedArrival": "2026-09-26T16:56:01+08:00", "Label": ""},
          "NextBus2": {"EstimatedArrival": "2026-09-26T17:15:59+08:00", "Label": "To Shenton Way Ter"},
          "NextBus3": {"EstimatedArrival": "", "Label": ""}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_STRING("", result.services[0].labels[0].c_str());
    TEST_ASSERT_EQUAL_STRING("To Shenton Way Ter", result.services[0].labels[1].c_str());
}

void test_strip_to_prefix_removes_to_space() {
    TEST_ASSERT_EQUAL_STRING("St. Michael's Ter", stripToPrefix("To St. Michael's Ter").c_str());
    TEST_ASSERT_EQUAL_STRING("Sims", stripToPrefix("To Sims").c_str());
    TEST_ASSERT_EQUAL_STRING("", stripToPrefix("").c_str());
    TEST_ASSERT_EQUAL_STRING("NoSpace", stripToPrefix("NoSpace").c_str());
}

void test_flatten_groups_by_service_and_label() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [
          {
            "ServiceNo": "124",
            "NextBus": {"EstimatedArrival": "2026-09-26T16:41:44+08:00", "Label": "To St. Michael's Ter"},
            "NextBus2": {"EstimatedArrival": "2026-09-26T16:57:11+08:00", "Label": "To St. Michael's Ter"}
          },
          {
            "ServiceNo": "124",
            "NextBus": {"EstimatedArrival": "2026-09-26T16:46:01+08:00", "Label": "To HarbourFront Int"},
            "NextBus2": {"EstimatedArrival": "2026-09-26T16:58:01+08:00", "Label": "To HarbourFront Int"}
          }
        ]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(2, result.rows.size());
    TEST_ASSERT_EQUAL_STRING("124", result.rows[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("St. Michael's Ter", result.rows[0].label.c_str());
    TEST_ASSERT_EQUAL_STRING("124", result.rows[1].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("HarbourFront Int", result.rows[1].label.c_str());
}

void test_flatten_sorts_arrivals_by_eta() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "139",
          "NextBus": {"EstimatedArrival": "2026-09-26T16:51:31+08:00", "Label": "To Toa Payoh Int"},
          "NextBus2": {"EstimatedArrival": "2026-09-26T16:41:27+08:00", "Label": "To Toa Payoh Int"},
          "NextBus3": {"EstimatedArrival": "2026-09-26T17:06:05+08:00", "Label": "To Toa Payoh Int"}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(1, result.rows.size());
    // Should be sorted: 16:41, 16:51, 17:06
    int64_t eta0 = result.rows[0].arrivals[0].etaEpoch;
    int64_t eta1 = result.rows[0].arrivals[1].etaEpoch;
    int64_t eta2 = result.rows[0].arrivals[2].etaEpoch;
    TEST_ASSERT_TRUE(eta0 < eta1);
    TEST_ASSERT_TRUE(eta1 < eta2);
}

void test_flatten_skips_empty_slots() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "186",
          "NextBus": {"EstimatedArrival": "2026-09-26T16:56:01+08:00", "Label": "To Shenton Way Ter"},
          "NextBus2": {"EstimatedArrival": "2026-09-26T17:15:59+08:00", "Label": "To Shenton Way Ter"},
          "NextBus3": {"EstimatedArrival": "", "Label": ""}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(1, result.rows.size());
    TEST_ASSERT_TRUE(result.rows[0].arrivals[0].etaEpoch > 0);
    TEST_ASSERT_TRUE(result.rows[0].arrivals[1].etaEpoch > 0);
    TEST_ASSERT_EQUAL_INT64(-1, result.rows[0].arrivals[2].etaEpoch);
}

void test_flatten_clears_labels_for_single_direction_services() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "186",
          "NextBus": {"EstimatedArrival": "2026-09-26T16:56:01+08:00", "Label": "To Shenton Way Ter"}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(1, result.rows.size());
    // Single direction non-loop service should have empty label
    TEST_ASSERT_EQUAL_STRING("", result.rows[0].label.c_str());
}

void test_flatten_keeps_labels_for_loops() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "125",
          "Loop": {"IsLoop": true},
          "NextBus": {"EstimatedArrival": "2026-09-26T16:47:10+08:00", "Label": "To Sims"}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(1, result.rows.size());
    // Loop service should keep its label even if only one row
    TEST_ASSERT_EQUAL_STRING("Sims", result.rows[0].label.c_str());
    TEST_ASSERT_TRUE(result.rows[0].isLoop);
}

void test_v2_empty_services_array() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": []
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(0, result.rows.size());
}

void test_loop_visit_1_rows_before_visit_2() {
    // Service 125 at stop 52109: NextBus is visit 2, later arrivals are visit 1
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [
          {
            "ServiceNo": "125",
            "Loop": {"IsLoop": true},
            "NextBus": {"EstimatedArrival": "2026-09-26T16:41:00+08:00", "Label": "To St. Michael's Ter", "VisitNumber": "2"},
            "NextBus2": {"EstimatedArrival": "2026-09-26T16:56:00+08:00", "Label": "To Sims", "VisitNumber": "1"}
          },
          {
            "ServiceNo": "131",
            "Loop": {"IsLoop": true},
            "NextBus": {"EstimatedArrival": "2026-09-26T16:42:00+08:00", "Label": "To St. Michael's Ter", "VisitNumber": "2"},
            "NextBus2": {"EstimatedArrival": "2026-09-26T16:48:00+08:00", "Label": "To Bt Merah Int", "VisitNumber": "1"}
          }
        ]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(4, result.rows.size());
    
    // Service 125: visit-1 row (Sims) should come before visit-2 row (St. Michael's Ter)
    TEST_ASSERT_EQUAL_STRING("125", result.rows[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("Sims", result.rows[0].label.c_str());
    TEST_ASSERT_EQUAL_STRING("125", result.rows[1].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("St. Michael's Ter", result.rows[1].label.c_str());
    
    // Service 131: visit-1 row (Bt Merah Int) should come before visit-2 row (St. Michael's Ter)
    TEST_ASSERT_EQUAL_STRING("131", result.rows[2].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("Bt Merah Int", result.rows[2].label.c_str());
    TEST_ASSERT_EQUAL_STRING("131", result.rows[3].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("St. Michael's Ter", result.rows[3].label.c_str());
}

void test_rows_keyed_by_service_and_label_merge_visits() {
    // Same label from different visits should merge into one row
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [
          {
            "ServiceNo": "125",
            "Loop": {"IsLoop": true},
            "NextBus": {"EstimatedArrival": "2026-09-26T16:41:00+08:00", "Label": "To Sims", "VisitNumber": "1"},
            "NextBus2": {"EstimatedArrival": "2026-09-26T16:51:00+08:00", "Label": "To Sims", "VisitNumber": "2"}
          }
        ]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    // Should have 1 row, not 2 (both visits merged because same label)
    TEST_ASSERT_EQUAL(1, result.rows.size());
    TEST_ASSERT_EQUAL_STRING("125", result.rows[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("Sims", result.rows[0].label.c_str());
    // Both arrivals should be in the row, sorted by time
    TEST_ASSERT_TRUE(result.rows[0].arrivals[0].etaEpoch > 0);
    TEST_ASSERT_TRUE(result.rows[0].arrivals[1].etaEpoch > 0);
    TEST_ASSERT_TRUE(result.rows[0].arrivals[0].etaEpoch < result.rows[0].arrivals[1].etaEpoch);
}

void test_empty_label_merges_into_single_non_empty_label() {
    // Empty-label arrivals should merge into the single non-empty label row
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "186",
          "NextBus": {"EstimatedArrival": "2026-09-26T16:40:00+08:00", "Label": ""},
          "NextBus2": {"EstimatedArrival": "2026-09-26T16:50:00+08:00", "Label": "To Shenton Way Ter"},
          "NextBus3": {"EstimatedArrival": "2026-09-26T17:00:00+08:00", "Label": ""}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    // Should have 1 row (empty labels merged into the single non-empty label)
    TEST_ASSERT_EQUAL(1, result.rows.size());
    TEST_ASSERT_EQUAL_STRING("186", result.rows[0].serviceNo.c_str());
    // All three arrivals should be merged and sorted by time
    TEST_ASSERT_TRUE(result.rows[0].arrivals[0].etaEpoch > 0);
    TEST_ASSERT_TRUE(result.rows[0].arrivals[1].etaEpoch > 0);
    TEST_ASSERT_TRUE(result.rows[0].arrivals[2].etaEpoch > 0);
    TEST_ASSERT_TRUE(result.rows[0].arrivals[0].etaEpoch < result.rows[0].arrivals[1].etaEpoch);
    TEST_ASSERT_TRUE(result.rows[0].arrivals[1].etaEpoch < result.rows[0].arrivals[2].etaEpoch);
}

void test_labels_shown_for_multiple_distinct_labels() {
    // Service with 2+ distinct non-empty labels should show labels
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [
          {
            "ServiceNo": "124",
            "NextBus": {"EstimatedArrival": "2026-09-26T16:41:00+08:00", "Label": "To St. Michael's Ter"}
          },
          {
            "ServiceNo": "124",
            "NextBus": {"EstimatedArrival": "2026-09-26T16:46:00+08:00", "Label": "To HarbourFront Int"}
          }
        ]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(2, result.rows.size());
    // Both rows should have labels visible
    TEST_ASSERT_EQUAL_STRING("St. Michael's Ter", result.rows[0].label.c_str());
    TEST_ASSERT_EQUAL_STRING("HarbourFront Int", result.rows[1].label.c_str());
}

void test_labels_hidden_for_single_direction_non_loop() {
    // Service with only one label and not a loop should hide the label
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "186",
          "Loop": {"IsLoop": false},
          "NextBus": {"EstimatedArrival": "2026-09-26T16:56:00+08:00", "Label": "To Shenton Way Ter"}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(1, result.rows.size());
    // Label should be hidden (empty) for single-direction non-loop
    TEST_ASSERT_EQUAL_STRING("", result.rows[0].label.c_str());
}

void test_labels_shown_for_loops_even_with_one_label() {
    // Loop service should show label even with only one distinct label
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "125",
          "Loop": {"IsLoop": true},
          "NextBus": {"EstimatedArrival": "2026-09-26T16:47:00+08:00", "Label": "To Sims"}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(1, result.rows.size());
    // Label should be visible for loop service
    TEST_ASSERT_EQUAL_STRING("Sims", result.rows[0].label.c_str());
    TEST_ASSERT_TRUE(result.rows[0].isLoop);
}

void test_service_with_no_arrivals_kept_as_row() {
    // Service with no arrivals should still create a row with all empty arrivals
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "999",
          "NextBus": {"EstimatedArrival": "", "Label": ""},
          "NextBus2": {"EstimatedArrival": "", "Label": ""},
          "NextBus3": {"EstimatedArrival": "", "Label": ""}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(1, result.rows.size());
    TEST_ASSERT_EQUAL_STRING("999", result.rows[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("", result.rows[0].label.c_str());
    // All arrivals should be empty (etaEpoch = -1)
    TEST_ASSERT_EQUAL_INT64(-1, result.rows[0].arrivals[0].etaEpoch);
    TEST_ASSERT_EQUAL_INT64(-1, result.rows[0].arrivals[1].etaEpoch);
    TEST_ASSERT_EQUAL_INT64(-1, result.rows[0].arrivals[2].etaEpoch);
}

void test_service_with_no_arrivals_across_multiple_entries() {
    // Service appearing multiple times (different destinations) with no arrivals
    // should create ONE empty row, not multiple
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [
          {
            "ServiceNo": "123",
            "NextBus": {"EstimatedArrival": "", "Label": "To North"},
            "NextBus2": {"EstimatedArrival": "", "Label": ""},
            "NextBus3": {"EstimatedArrival": "", "Label": ""}
          },
          {
            "ServiceNo": "123",
            "NextBus": {"EstimatedArrival": "", "Label": "To South"},
            "NextBus2": {"EstimatedArrival": "", "Label": ""},
            "NextBus3": {"EstimatedArrival": "", "Label": ""}
          }
        ]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(1, result.rows.size());
    TEST_ASSERT_EQUAL_STRING("123", result.rows[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("", result.rows[0].label.c_str());
    TEST_ASSERT_EQUAL_INT64(-1, result.rows[0].arrivals[0].etaEpoch);
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_service_and_three_etas);
    RUN_TEST(test_parses_load_for_each_arrival);
    RUN_TEST(test_parses_vehicle_type_for_each_arrival);
    RUN_TEST(test_vehicle_type_codes_map_to_decks);
    RUN_TEST(test_unrecognised_vehicle_type_is_unknown);
    RUN_TEST(test_missing_vehicle_type_field_is_unknown);
    RUN_TEST(test_load_codes_map_to_crowding_levels);
    RUN_TEST(test_unrecognised_load_is_unknown);
    RUN_TEST(test_missing_load_field_is_unknown);
    RUN_TEST(test_missing_nextbus2_and_3_return_negative_one);
    RUN_TEST(test_empty_bus_stops_is_invalid);
    RUN_TEST(test_malformed_json_is_invalid);
    RUN_TEST(test_accepts_numeric_bus_stop_code);
    RUN_TEST(test_matches_numeric_bus_stop_code_with_leading_zero);
    RUN_TEST(test_select_display_services_caps_at_six);
    RUN_TEST(test_page_count_rounds_up);
    RUN_TEST(test_second_page_continues_where_first_ended);
    RUN_TEST(test_page_past_the_end_is_empty);
    // V2 API tests
    RUN_TEST(test_v2_parses_string_bus_stop_code);
    RUN_TEST(test_v2_parses_label_and_visit_number);
    RUN_TEST(test_v2_empty_label_is_parsed);
    RUN_TEST(test_strip_to_prefix_removes_to_space);
    RUN_TEST(test_flatten_groups_by_service_and_label);
    RUN_TEST(test_flatten_sorts_arrivals_by_eta);
    RUN_TEST(test_flatten_skips_empty_slots);
    RUN_TEST(test_flatten_clears_labels_for_single_direction_services);
    RUN_TEST(test_flatten_keeps_labels_for_loops);
    RUN_TEST(test_v2_empty_services_array);
    RUN_TEST(test_loop_visit_1_rows_before_visit_2);
    RUN_TEST(test_rows_keyed_by_service_and_label_merge_visits);
    RUN_TEST(test_empty_label_merges_into_single_non_empty_label);
    RUN_TEST(test_labels_shown_for_multiple_distinct_labels);
    RUN_TEST(test_labels_hidden_for_single_direction_non_loop);
    RUN_TEST(test_labels_shown_for_loops_even_with_one_label);
    RUN_TEST(test_service_with_no_arrivals_kept_as_row);
    RUN_TEST(test_service_with_no_arrivals_across_multiple_entries);
    return UNITY_END();
}
