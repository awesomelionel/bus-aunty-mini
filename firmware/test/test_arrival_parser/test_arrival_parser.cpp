#include <fstream>
#include <sstream>
#include <string>

#include <unity.h>

#include "core/arrival_parser.h"
#include "core/iso8601.h"
#include "core/night_window.h"

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

static std::vector<BusServiceRow> makeRows(size_t count) {
    std::vector<BusServiceRow> rows;
    for (size_t i = 0; i < count; ++i) {
        BusServiceRow row;
        row.serviceNo = std::to_string(i);
        rows.push_back(row);
    }
    return rows;
}

void test_select_display_services_caps_at_six() {
    std::vector<BusServiceRow> rows = makeRows(9);
    std::vector<BusServiceRow> selected = selectServicePage(rows, 6, 0);
    TEST_ASSERT_EQUAL(6, selected.size());
    TEST_ASSERT_EQUAL_STRING("0", selected[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("5", selected[5].serviceNo.c_str());
}

void test_page_count_rounds_up() {
    TEST_ASSERT_EQUAL_UINT32(0, servicePageCount(makeRows(0), 6));
    TEST_ASSERT_EQUAL_UINT32(1, servicePageCount(makeRows(1), 6));
    TEST_ASSERT_EQUAL_UINT32(1, servicePageCount(makeRows(6), 6));
    TEST_ASSERT_EQUAL_UINT32(2, servicePageCount(makeRows(7), 6));
    TEST_ASSERT_EQUAL_UINT32(3, servicePageCount(makeRows(13), 6));
}

void test_second_page_continues_where_first_ended() {
    std::vector<BusServiceRow> rows = makeRows(8);
    std::vector<BusServiceRow> page = selectServicePage(rows, 6, 1);
    TEST_ASSERT_EQUAL_UINT32(2, page.size());
    TEST_ASSERT_EQUAL_STRING("6", page[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("7", page[1].serviceNo.c_str());
}

void test_page_past_the_end_is_empty() {
    std::vector<BusServiceRow> rows = makeRows(8);
    TEST_ASSERT_EQUAL_UINT32(0, selectServicePage(rows, 6, 2).size());
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
    TEST_ASSERT_EQUAL_STRING("HarbourFront Int", result.rows[0].label.c_str());
    TEST_ASSERT_EQUAL_STRING("124", result.rows[1].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("St. Michael's Ter", result.rows[1].label.c_str());
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

void test_labels_kept_for_single_direction_services() {
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
    TEST_ASSERT_EQUAL_STRING("Shenton Way Ter", result.rows[0].label.c_str());
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

void test_same_label_different_visits_stay_separate() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [
          {
            "ServiceNo": "125",
            "Loop": {"IsLoop": true},
            "NextBus": {"EstimatedArrival": "2026-09-26T16:51:00+08:00", "Label": "To Sims", "VisitNumber": "2"},
            "NextBus2": {"EstimatedArrival": "2026-09-26T16:41:00+08:00", "Label": "To Sims", "VisitNumber": "1"}
          }
        ]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(2, result.rows.size());
    TEST_ASSERT_EQUAL_STRING("1", result.rows[0].visitNumber.c_str());
    TEST_ASSERT_EQUAL_STRING("Sims", result.rows[0].label.c_str());
    TEST_ASSERT_EQUAL_STRING("2", result.rows[1].visitNumber.c_str());
    TEST_ASSERT_TRUE(shouldShowVisit2Marker(result.rows[1]));
    TEST_ASSERT_FALSE(shouldShowVisit2Marker(result.rows[0]));
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
    // Same visit, so labels sort alphabetically rather than by API slot.
    TEST_ASSERT_EQUAL_STRING("HarbourFront Int", result.rows[0].label.c_str());
    TEST_ASSERT_EQUAL_STRING("St. Michael's Ter", result.rows[1].label.c_str());
}

void test_labels_shown_for_single_direction_non_loop() {
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
    TEST_ASSERT_EQUAL_STRING("Shenton Way Ter", result.rows[0].label.c_str());
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

void test_paging_avoids_straddling_multirow_services() {
    // Service 111 has 2 rows, service 222 has 1 row, service 333 has 2 rows
    // With pageSize=3, naive paging would put 111(2) + 222(1) on page 0, 333(2) on page 1
    // But 333's 2 rows would straddle if we only fit 1 on page 0
    // Smart paging should put 111(2) + 222(1) on page 0, 333(2) on page 1
    
    std::vector<BusServiceRow> rows;
    
    // Service 111 with 2 rows (different destinations)
    BusServiceRow row1;
    row1.serviceNo = "111";
    row1.label = "North";
    rows.push_back(row1);
    
    BusServiceRow row2;
    row2.serviceNo = "111";
    row2.label = "South";
    rows.push_back(row2);
    
    // Service 222 with 1 row
    BusServiceRow row3;
    row3.serviceNo = "222";
    row3.label = "";
    rows.push_back(row3);
    
    // Service 333 with 2 rows (different destinations)
    BusServiceRow row4;
    row4.serviceNo = "333";
    row4.label = "East";
    rows.push_back(row4);
    
    BusServiceRow row5;
    row5.serviceNo = "333";
    row5.label = "West";
    rows.push_back(row5);
    
    // With pageSize=3, we should get 2 pages
    size_t pageCount = servicePageCount(rows, 3);
    TEST_ASSERT_EQUAL(2, pageCount);
    
    // Page 0: 111(2) + 222(1) = 3 rows
    std::vector<BusServiceRow> page0 = selectServicePage(rows, 3, 0);
    TEST_ASSERT_EQUAL(3, page0.size());
    TEST_ASSERT_EQUAL_STRING("111", page0[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("111", page0[1].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("222", page0[2].serviceNo.c_str());
    
    // Page 1: 333(2) = 2 rows (not straddled)
    std::vector<BusServiceRow> page1 = selectServicePage(rows, 3, 1);
    TEST_ASSERT_EQUAL(2, page1.size());
    TEST_ASSERT_EQUAL_STRING("333", page1[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("333", page1[1].serviceNo.c_str());
}

void test_paging_that_would_split_service() {
    // Old naive paging WOULD split: service 111(2) + 222(2) with pageSize=3
    // Old: page0=[111(2) + 222(1st)], page1=[222(2nd)] - splits 222!
    // New: page0=[111(2)], page1=[222(2)] - keeps 222 together
    
    std::vector<BusServiceRow> rows;
    
    BusServiceRow row1;
    row1.serviceNo = "111";
    row1.label = "North";
    rows.push_back(row1);
    
    BusServiceRow row2;
    row2.serviceNo = "111";
    row2.label = "South";
    rows.push_back(row2);
    
    BusServiceRow row3;
    row3.serviceNo = "222";
    row3.label = "East";
    rows.push_back(row3);
    
    BusServiceRow row4;
    row4.serviceNo = "222";
    row4.label = "West";
    rows.push_back(row4);
    
    // Should get 2 pages
    size_t pageCount = servicePageCount(rows, 3);
    TEST_ASSERT_EQUAL(2, pageCount);
    
    // Page 0: only 111(2) to avoid splitting 222
    std::vector<BusServiceRow> page0 = selectServicePage(rows, 3, 0);
    TEST_ASSERT_EQUAL(2, page0.size());
    TEST_ASSERT_EQUAL_STRING("111", page0[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("111", page0[1].serviceNo.c_str());
    
    // Page 1: all of 222(2)
    std::vector<BusServiceRow> page1 = selectServicePage(rows, 3, 1);
    TEST_ASSERT_EQUAL(2, page1.size());
    TEST_ASSERT_EQUAL_STRING("222", page1[0].serviceNo.c_str());
    TEST_ASSERT_EQUAL_STRING("222", page1[1].serviceNo.c_str());
}

void test_no_empty_row_for_service_with_mixed_entries() {
    // Service 123 appears twice: once with no arrivals, once with arrivals
    // Should NOT create a "--" row since service HAS arrivals in at least one entry
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [
          {
            "ServiceNo": "123",
            "NextBus": {"EstimatedArrival": "", "Label": ""},
            "NextBus2": {"EstimatedArrival": "", "Label": ""},
            "NextBus3": {"EstimatedArrival": "", "Label": ""}
          },
          {
            "ServiceNo": "123",
            "NextBus": {"EstimatedArrival": "2024-01-15T10:30:00+08:00", "Label": "To North"},
            "NextBus2": {"EstimatedArrival": "", "Label": ""},
            "NextBus3": {"EstimatedArrival": "", "Label": ""}
          }
        ]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL(1, result.rows.size());  // Only one row, NOT two (no "--" row)
    TEST_ASSERT_EQUAL_STRING("123", result.rows[0].serviceNo.c_str());
    TEST_ASSERT_TRUE(result.rows[0].arrivals[0].etaEpoch > 0);  // Has arrival
}

void test_should_show_visit2_marker() {
    BusServiceRow row;
    row.serviceNo = "125";
    row.label = "Sims";

    TEST_ASSERT_FALSE(shouldShowVisit2Marker(row));

    row.arrivals[0].etaEpoch = 1000;
    row.arrivals[0].visitNumber = "1";
    row.arrivals[1].etaEpoch = 2000;
    row.arrivals[1].visitNumber = "2";
    TEST_ASSERT_FALSE(shouldShowVisit2Marker(row));

    row.arrivals[0].visitNumber = "2";
    row.arrivals[1].visitNumber = "2";
    TEST_ASSERT_TRUE(shouldShowVisit2Marker(row));

    row.label.clear();
    TEST_ASSERT_FALSE(shouldShowVisit2Marker(row));
}

void test_empty_labels_from_different_entries_stay_separate() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [
          {"ServiceNo": "10", "NextBus": {"EstimatedArrival": "2026-09-26T16:40:00+08:00", "Label": "", "VisitNumber": "1"}},
          {"ServiceNo": "10", "NextBus": {"EstimatedArrival": "2026-09-26T16:50:00+08:00", "Label": "", "VisitNumber": "1"}}
        ]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_EQUAL(2, result.rows.size());
    TEST_ASSERT_EQUAL_STRING("", result.rows[0].label.c_str());
    TEST_ASSERT_EQUAL_STRING("", result.rows[1].label.c_str());
    TEST_ASSERT_TRUE(result.rows[0].arrivals[0].etaEpoch < result.rows[1].arrivals[0].etaEpoch);
}

void test_loop_desc_fills_empty_label() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "125",
          "Loop": {"IsLoop": true, "LoopDesc": "Sims Dr"},
          "NextBus": {"EstimatedArrival": "2026-09-26T16:40:00+08:00", "Label": "", "VisitNumber": "1"}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_EQUAL(1, result.rows.size());
    TEST_ASSERT_EQUAL_STRING("Sims Dr", result.rows[0].label.c_str());
    TEST_ASSERT_TRUE(result.rows[0].isLoop);
}

void test_null_loop_desc_leaves_label_blank() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "125",
          "Loop": {"IsLoop": true, "LoopDesc": null},
          "NextBus": {"EstimatedArrival": "2026-09-26T16:40:00+08:00", "Label": "", "VisitNumber": "1"}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_EQUAL_STRING("", result.rows[0].label.c_str());
}

void test_terminating_is_parsed_and_marks_ends_here() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "UpdatedAt": "2026-09-28T16:33:03.630381+08:00",
        "Services": [{
          "ServiceNo": "12",
          "NextBus": {"EstimatedArrival": "2026-09-28T16:40:00+08:00", "Label": "To Depot", "VisitNumber": "2", "Terminating": true},
          "NextBus2": {"EstimatedArrival": "2026-09-28T16:50:00+08:00", "Label": "To Depot", "VisitNumber": "2", "Terminating": true}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.rows[0].arrivals[0].terminating);
    TEST_ASSERT_TRUE(result.rows[0].arrivals[1].terminating);
    TEST_ASSERT_TRUE(rowAllTerminating(result.rows[0]));
    TEST_ASSERT_FALSE(shouldShowVisit2Marker(result.rows[0]));
    TEST_ASSERT_TRUE(result.updatedAtEpoch > 0);
    TEST_ASSERT_EQUAL_INT64(
        parseIso8601ToEpoch("2026-09-28T16:33:03+08:00"), result.updatedAtEpoch);
}

void test_mixed_terminating_does_not_replace_label() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [{
          "ServiceNo": "10",
          "NextBus": {"EstimatedArrival": "2026-09-28T16:40:00+08:00", "Label": "To HarbourFront Int", "VisitNumber": "1", "Terminating": true},
          "NextBus2": {"EstimatedArrival": "2026-09-28T16:50:00+08:00", "Label": "To HarbourFront Int", "VisitNumber": "1", "Terminating": false}
        }]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(result.rows[0].arrivals[0].terminating);
    TEST_ASSERT_FALSE(result.rows[0].arrivals[1].terminating);
    TEST_ASSERT_FALSE(rowAllTerminating(result.rows[0]));
    TEST_ASSERT_EQUAL_STRING("HarbourFront Int", result.rows[0].label.c_str());
}

void test_prune_drops_buses_more_than_two_minutes_past() {
    const int64_t now = kClockSetEpoch + 5000;
    BusServiceRow row;
    row.serviceNo = "125";
    row.label = "Sims";
    row.arrivals[0].etaEpoch = now - 121;
    row.arrivals[1].etaEpoch = now - 240;
    row.arrivals[2].etaEpoch = now + 90;
    std::vector<BusServiceRow> rows = {row};
    pruneExpiredArrivals(rows, now);
    TEST_ASSERT_EQUAL(1, rows.size());
    TEST_ASSERT_EQUAL_INT64(now + 90, rows[0].arrivals[0].etaEpoch);
    TEST_ASSERT_EQUAL_INT64(-1, rows[0].arrivals[1].etaEpoch);
}

void test_prune_skips_a_row_with_nothing_left() {
    const int64_t now = kClockSetEpoch + 5000;
    BusServiceRow row;
    row.serviceNo = "125";
    row.label = "Sims";
    row.arrivals[0].etaEpoch = now - 121;
    std::vector<BusServiceRow> rows = {row};
    pruneExpiredArrivals(rows, now);
    TEST_ASSERT_EQUAL(0, rows.size());
}

void test_prune_keeps_a_bus_exactly_two_minutes_past() {
    const int64_t now = kClockSetEpoch + 5000;
    BusServiceRow row;
    row.serviceNo = "125";
    row.arrivals[0].etaEpoch = now - 120;
    std::vector<BusServiceRow> rows = {row};
    pruneExpiredArrivals(rows, now);
    TEST_ASSERT_EQUAL(1, rows.size());
}

void test_prune_does_nothing_when_the_clock_is_unset() {
    BusServiceRow row;
    row.serviceNo = "125";
    row.arrivals[0].etaEpoch = 100;
    std::vector<BusServiceRow> rows = {row};
    pruneExpiredArrivals(rows, 1000);
    TEST_ASSERT_EQUAL(1, rows.size());
}

void test_cap_keeps_the_earliest_three() {
    const char* json = R"JSON({
      "busStops": [{
        "BusStopCode": "52109",
        "Services": [
          {"ServiceNo": "124", "NextBus": {"EstimatedArrival": "2026-09-26T16:40:00+08:00", "Label": "To Sims", "VisitNumber": "1"},
           "NextBus2": {"EstimatedArrival": "2026-09-26T16:50:00+08:00", "Label": "To Sims", "VisitNumber": "1"},
           "NextBus3": {"EstimatedArrival": "2026-09-26T17:00:00+08:00", "Label": "To Sims", "VisitNumber": "1"}},
          {"ServiceNo": "124", "NextBus": {"EstimatedArrival": "2026-09-26T16:30:00+08:00", "Label": "To Sims", "VisitNumber": "1"}}
        ]
      }]
    })JSON";
    ParsedBusStop result = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_EQUAL(1, result.rows.size());
    TEST_ASSERT_TRUE(result.rows[0].arrivals[0].etaEpoch < result.rows[0].arrivals[1].etaEpoch);
    TEST_ASSERT_TRUE(result.rows[0].arrivals[1].etaEpoch < result.rows[0].arrivals[2].etaEpoch);
    int64_t latest = result.rows[0].arrivals[2].etaEpoch;
    int64_t fourth = parseIso8601ToEpoch("2026-09-26T17:00:00+08:00");
    TEST_ASSERT_TRUE(latest < fourth);
}

void test_row_order_ignores_which_slot_is_first() {
    const char* leadA = R"JSON({
      "busStops": [{"BusStopCode": "52109", "Services": [{
        "ServiceNo": "124",
        "NextBus": {"EstimatedArrival": "2026-09-28T16:40:00+08:00", "Label": "To St. Michael's Ter", "VisitNumber": "1"},
        "NextBus2": {"EstimatedArrival": "2026-09-28T16:50:00+08:00", "Label": "To HarbourFront Int", "VisitNumber": "1"}
      }]}]
    })JSON";
    const char* leadB = R"JSON({
      "busStops": [{"BusStopCode": "52109", "Services": [{
        "ServiceNo": "124",
        "NextBus": {"EstimatedArrival": "2026-09-28T16:50:00+08:00", "Label": "To HarbourFront Int", "VisitNumber": "1"},
        "NextBus2": {"EstimatedArrival": "2026-09-28T16:40:00+08:00", "Label": "To St. Michael's Ter", "VisitNumber": "1"}
      }]}]
    })JSON";
    ParsedBusStop a = parseBusArrivalResponse(leadA, "52109");
    ParsedBusStop b = parseBusArrivalResponse(leadB, "52109");
    TEST_ASSERT_EQUAL(2, a.rows.size());
    TEST_ASSERT_EQUAL_STRING(a.rows[0].label.c_str(), b.rows[0].label.c_str());
    TEST_ASSERT_EQUAL_STRING(a.rows[1].label.c_str(), b.rows[1].label.c_str());
    TEST_ASSERT_EQUAL_STRING("HarbourFront Int", a.rows[0].label.c_str());
}

void test_service_and_row_caps_hold() {
    std::string json = "{\"busStops\":[{\"BusStopCode\":\"52109\",\"Services\":[";
    for (int i = 0; i < 3000; ++i) {
        if (i) {
            json += ',';
        }
        json += "{\"ServiceNo\":\"" + std::to_string(i) +
                "\",\"Loop\":{\"IsLoop\":false,\"LoopDesc\":null},"
                "\"NextBus\":{\"EstimatedArrival\":\"2026-09-28T16:40:00+08:00\","
                "\"Label\":\"To A\",\"VisitNumber\":\"1\",\"Load\":\"SEA\","
                "\"Type\":\"SD\",\"Terminating\":false},"
                "\"NextBus2\":{\"EstimatedArrival\":\"\",\"Label\":\"\","
                "\"VisitNumber\":\"\",\"Load\":\"\",\"Type\":\"\",\"Terminating\":false},"
                "\"NextBus3\":{\"EstimatedArrival\":\"\",\"Label\":\"\","
                "\"VisitNumber\":\"\",\"Load\":\"\",\"Type\":\"\",\"Terminating\":false}}";
    }
    json += "]}]}";
    ParsedBusStop capped = parseBusArrivalResponse(json, "52109");
    TEST_ASSERT_TRUE(capped.valid);
    TEST_ASSERT_EQUAL(kMaxServicesPerStop, capped.services.size());
    TEST_ASSERT_TRUE(capped.rows.size() <= kMaxRowsPerStop);
    TEST_ASSERT_TRUE(capped.rows.size() < 3000);

    std::string wide = "{\"busStops\":[{\"BusStopCode\":\"52109\",\"Services\":[";
    for (int i = 0; i < 40; ++i) {
        if (i) {
            wide += ',';
        }
        const std::string no = std::to_string(i);
        wide += "{\"ServiceNo\":\"" + no +
                "\",\"NextBus\":{\"EstimatedArrival\":\"2026-09-28T16:40:00+08:00\","
                "\"Label\":\"To A\",\"VisitNumber\":\"1\"},"
                "\"NextBus2\":{\"EstimatedArrival\":\"2026-09-28T16:50:00+08:00\","
                "\"Label\":\"To B\",\"VisitNumber\":\"1\"},"
                "\"NextBus3\":{\"EstimatedArrival\":\"2026-09-28T17:00:00+08:00\","
                "\"Label\":\"To C\",\"VisitNumber\":\"1\"}}";
    }
    wide += "]}]}";
    ParsedBusStop rows = parseBusArrivalResponse(wide, "52109");
    TEST_ASSERT_EQUAL(40, rows.services.size());
    TEST_ASSERT_EQUAL(kMaxRowsPerStop, rows.rows.size());
}

static std::string readFixture(const char* name) {
    std::ifstream in(std::string("test/data/") + name);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

static const BusServiceRow* findRow(const ParsedBusStop& stop, const char* service,
                                    const char* visit) {
    for (const BusServiceRow& row : stop.rows) {
        if (row.serviceNo == service && row.visitNumber == visit) {
            return &row;
        }
    }
    return nullptr;
}

void test_live_52109_rows() {
    ParsedBusStop stop = parseBusArrivalResponse(readFixture("stop_52109.json"), "52109");
    TEST_ASSERT_TRUE(stop.valid);
    TEST_ASSERT_TRUE(stop.updatedAtEpoch > 0);
    int labels124 = 0;
    bool saw125Visit2 = false;
    bool saw125Loop = false;
    for (const BusServiceRow& row : stop.rows) {
        if (row.serviceNo == "124" && !row.label.empty()) {
            ++labels124;
        }
        if (row.serviceNo == "125" && row.visitNumber == "2") {
            saw125Visit2 = true;
            saw125Loop = row.isLoop;
            TEST_ASSERT_FALSE(row.label.empty());
        }
    }
    TEST_ASSERT_EQUAL(2, labels124);
    TEST_ASSERT_TRUE(saw125Visit2);
    TEST_ASSERT_TRUE(saw125Loop);
    TEST_ASSERT_TRUE(servicePageCount(stop.rows, 4) >= 3);
}

void test_live_52049_and_66271() {
    ParsedBusStop a = parseBusArrivalResponse(readFixture("stop_52049.json"), "52049");
    ParsedBusStop b = parseBusArrivalResponse(readFixture("stop_66271.json"), "66271");
    TEST_ASSERT_TRUE(a.valid);
    TEST_ASSERT_TRUE(b.valid);
    int labels21 = 0;
    int labels129 = 0;
    int labels136 = 0;
    for (const BusServiceRow& row : a.rows) {
        if (row.serviceNo == "21") ++labels21;
        if (row.serviceNo == "129") ++labels129;
    }
    for (const BusServiceRow& row : b.rows) {
        if (row.serviceNo == "136") ++labels136;
    }
    TEST_ASSERT_EQUAL(2, labels21);
    TEST_ASSERT_EQUAL(2, labels129);
    TEST_ASSERT_EQUAL(2, labels136);
}

void test_live_figure_eight_291_293() {
    ParsedBusStop stop = parseBusArrivalResponse(readFixture("figure_eight.json"), "75009");
    TEST_ASSERT_TRUE(stop.valid);
    const BusServiceRow* first = findRow(stop, "291", "1");
    const BusServiceRow* second = findRow(stop, "291", "2");
    TEST_ASSERT_TRUE(first != nullptr);
    TEST_ASSERT_TRUE(second != nullptr);
    TEST_ASSERT_TRUE(first->isLoop);
    TEST_ASSERT_TRUE(shouldShowVisit2Marker(*second));
    bool saw293 = false;
    for (const BusService& svc : stop.services) {
        if (svc.serviceNo == "293") {
            saw293 = true;
            TEST_ASSERT_TRUE(svc.isLoop);
        }
    }
    TEST_ASSERT_TRUE(saw293);
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
    RUN_TEST(test_labels_kept_for_single_direction_services);
    RUN_TEST(test_flatten_keeps_labels_for_loops);
    RUN_TEST(test_v2_empty_services_array);
    RUN_TEST(test_loop_visit_1_rows_before_visit_2);
    RUN_TEST(test_same_label_different_visits_stay_separate);
    RUN_TEST(test_empty_label_merges_into_single_non_empty_label);
    RUN_TEST(test_labels_shown_for_multiple_distinct_labels);
    RUN_TEST(test_labels_shown_for_single_direction_non_loop);
    RUN_TEST(test_labels_shown_for_loops_even_with_one_label);
    RUN_TEST(test_service_with_no_arrivals_kept_as_row);
    RUN_TEST(test_service_with_no_arrivals_across_multiple_entries);
    RUN_TEST(test_paging_avoids_straddling_multirow_services);
    RUN_TEST(test_paging_that_would_split_service);
    RUN_TEST(test_no_empty_row_for_service_with_mixed_entries);
    RUN_TEST(test_should_show_visit2_marker);
    RUN_TEST(test_empty_labels_from_different_entries_stay_separate);
    RUN_TEST(test_loop_desc_fills_empty_label);
    RUN_TEST(test_null_loop_desc_leaves_label_blank);
    RUN_TEST(test_terminating_is_parsed_and_marks_ends_here);
    RUN_TEST(test_mixed_terminating_does_not_replace_label);
    RUN_TEST(test_prune_drops_buses_more_than_two_minutes_past);
    RUN_TEST(test_prune_skips_a_row_with_nothing_left);
    RUN_TEST(test_prune_keeps_a_bus_exactly_two_minutes_past);
    RUN_TEST(test_prune_does_nothing_when_the_clock_is_unset);
    RUN_TEST(test_cap_keeps_the_earliest_three);
    RUN_TEST(test_row_order_ignores_which_slot_is_first);
    RUN_TEST(test_service_and_row_caps_hold);
    RUN_TEST(test_live_52109_rows);
    RUN_TEST(test_live_52049_and_66271);
    RUN_TEST(test_live_figure_eight_291_293);
    return UNITY_END();
}
