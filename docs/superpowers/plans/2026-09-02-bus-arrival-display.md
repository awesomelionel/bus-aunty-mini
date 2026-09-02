# Bus Arrival Display (M5Stack StickS3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build ESP32-S3 (M5Stack StickS3) firmware that polls `https://api.busaunty.com/api/v1/BusArrival` for one or more configured bus stop codes and shows up to 6 services per stop, all 3 arrival times each, on the device's 240x135 screen.

**Architecture:** Arduino/PlatformIO firmware split into pure logic modules (ISO-8601 parsing, ETA formatting, JSON parsing — unit-testable on the host via PlatformIO's `native` platform) and hardware-facing modules (HTTPS fetch, M5GFX display, WiFi captive-portal provisioning, button input) that only build for the ESP32-S3 target and are verified manually on-device. `main.cpp` polls the API every 30s, cycles between configured bus stops on a button press, and re-renders the screen with each result.

**Tech Stack:** PlatformIO, Arduino framework, M5Unified/M5GFX, ArduinoJson v7, WiFiManager (tzapu), HTTPClient + WiFiClientSecure, PlatformIO `native` + Unity for host-side unit tests.

**Validation performed while writing this plan:** PlatformIO was installed locally (`pip install platformio`) and every piece of code in this plan — the native unit tests *and* the full ESP32-S3 firmware build (`pio run -e sticks3`) — was actually compiled and run against the real M5Unified/M5GFX/WiFiManager/ArduinoJson library versions pinned below. All 12 native unit tests pass; the firmware builds successfully (Flash 44.6% used, RAM 16.5% used). The TLS root certificate in Task 7 was extracted from `api.busaunty.com`'s live certificate chain via `openssl s_client` on 2026-09-02, not guessed.

## Global Constraints

- Target hardware: M5Stack StickS3 — ESP32-S3-PICO-1-N8R8 (8MB flash, 8MB Octal PSRAM), 135x240 ST7789P3 LCD (rotated to 240x135 landscape), buttons KEY1=GPIO11 (`M5.BtnA`), KEY2=GPIO12 (`M5.BtnB`, unused by this plan).
- Framework: Arduino via PlatformIO. Board id `esp32-s3-devkitc-1` (no dedicated `m5stack-sticks3` PlatformIO board exists; PSRAM/flash are configured via `board_build.*` flags — see Task 1).
- API: `GET https://api.busaunty.com/api/v1/BusArrival?BusStopCode=<code>` — one code per request, response shape per `docs/README.md`. No changes to that API/service are in scope.
- Display up to 6 services per bus stop; show all 3 `NextBus`/`NextBus2`/`NextBus3` arrival times per service, converted to "minutes from now" (see Task 3) rather than raw timestamps, since raw ISO-8601 strings don't fit a 240x135 screen.
- WiFi credentials: on-device captive portal (WiFiManager), no credentials in source.
- Bus stop codes: compiled into `firmware/src/bus_stops.h` as a list; cycle between them by pressing KEY1 (`M5.BtnA`).
- Poll interval: 30 seconds: auto only, no manual-refresh button.
- Library versions (pinned, validated): `m5stack/M5Unified@^0.2.21`, `bblanchon/ArduinoJson@^7.2.2`, `tzapu/WiFiManager@^2.0.17`.
- Repo currently has no `.git` — Task 1 initializes it.

---

### Task 1: Project Scaffold

**Files:**
- Create: `firmware/platformio.ini`
- Create: `firmware/src/main.cpp`
- Create: `firmware/.gitignore`
- Create: `.gitignore` (repo root)

**Interfaces:**
- Consumes: nothing (first task).
- Produces: PlatformIO envs `sticks3` (ESP32-S3 firmware target) and `native` (host unit tests), used by every later task. `firmware/src/main.cpp` provides `setup()`/`loop()`, extended by later tasks.

- [ ] **Step 1: Initialize git and create the firmware directory**

```bash
cd /Users/lionel/Documents/code/bus-aunty-mini
git init
mkdir -p firmware/src firmware/test
```

- [ ] **Step 2: Create the root `.gitignore`**

```
# .gitignore
firmware/.pio/
firmware/.vscode/
.DS_Store
```

- [ ] **Step 3: Create `firmware/platformio.ini`**

```ini
[env:sticks3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.memory_type = qio_opi
board_build.psram_type = opi
board_upload.flash_size = 8MB
board_build.partitions = default_8MB.csv
monitor_speed = 115200
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
lib_deps =
    m5stack/M5Unified@^0.2.21
    bblanchon/ArduinoJson@^7.2.2
    tzapu/WiFiManager@^2.0.17

[env:native]
platform = native
test_framework = unity
test_build_src = yes
build_flags =
    -std=c++17
lib_deps =
    bblanchon/ArduinoJson@^7.2.2
build_src_filter =
    +<*>
    -<main.cpp>
    -<bus_api_client.cpp>
    -<display.cpp>
```

`test_build_src = yes` is required — without it, PlatformIO's `native` test runner only compiles the test file itself and every later task's native test fails to link with "undefined symbol" errors (confirmed while validating this plan).

- [ ] **Step 4: Create a minimal boot smoke-test `firmware/src/main.cpp`**

```cpp
#include <M5Unified.h>

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setCursor(4, 4);
    M5.Display.println("Bus Aunty booting...");
}

void loop() {
    M5.update();
}
```

- [ ] **Step 5: Build the firmware target**

Run: `cd firmware && ~/Library/Python/3.14/bin/pio run -e sticks3` (adjust the `pio` path to wherever `pip install platformio` put it, or use the `platformio` binary directly if it's on `PATH`)

Expected: ends with `[SUCCESS]` and a memory usage table, e.g.:
```
RAM:   [==        ]  16.5% (used 54000 bytes from 327680 bytes)
Flash: [====      ]  XX.X% (used ... bytes from 3342336 bytes)
========================= [SUCCESS] Took ...s =========================
```

- [ ] **Step 6: Flash and confirm the boot screen (hardware check)**

Connect the StickS3 via USB-C, then run: `pio run -e sticks3 -t upload -t monitor`

Expected: the device screen shows "Bus Aunty booting..." in the top-left corner. If the text appears upside-down or in portrait orientation, the device is mounted rotated 180°; change `M5.Display.setRotation(1)` to `setRotation(3)` everywhere it appears in this plan and reflash — this is a real, hardware-orientation-dependent choice, not something to guess from software alone.

- [ ] **Step 7: Commit**

```bash
git add .gitignore firmware/.gitignore firmware/platformio.ini firmware/src/main.cpp
git commit -m "feat: scaffold PlatformIO firmware project for StickS3"
```

---

### Task 2: ISO-8601 Timestamp Parsing

**Files:**
- Create: `firmware/src/iso8601.h`
- Create: `firmware/src/iso8601.cpp`
- Test: `firmware/test/test_iso8601/test_iso8601.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `int64_t parseIso8601ToEpoch(const std::string& iso8601)` — returns UTC epoch seconds for a `"YYYY-MM-DDTHH:MM:SSZ"` string, or `-1` if the string doesn't match that shape. Used by Task 4 (`arrival_parser.cpp`).

- [ ] **Step 1: Write the failing test**

```cpp
// firmware/test/test_iso8601/test_iso8601.cpp
#include <unity.h>

#include "iso8601.h"

void test_parses_valid_utc_timestamp() {
    int64_t epoch = parseIso8601ToEpoch("2024-03-20T12:34:56Z");
    TEST_ASSERT_EQUAL_INT64(1710938096LL, epoch);
}

void test_returns_negative_one_on_malformed_input() {
    TEST_ASSERT_EQUAL_INT64(-1, parseIso8601ToEpoch("not-a-date"));
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_valid_utc_timestamp);
    RUN_TEST(test_returns_negative_one_on_malformed_input);
    return UNITY_END();
}
```

- [ ] **Step 2: Create the header so the test fails to link, not to compile**

```cpp
// firmware/src/iso8601.h
#pragma once
#include <cstdint>
#include <string>

int64_t parseIso8601ToEpoch(const std::string& iso8601);
```

- [ ] **Step 3: Run the test to verify it fails**

Run: `cd firmware && pio test -e native -f test_iso8601`

Expected: `undefined symbols ... parseIso8601ToEpoch ... [ERRORED]` (no `.cpp` implementation exists yet).

- [ ] **Step 4: Implement `parseIso8601ToEpoch`**

Note: ESP32's Arduino/newlib toolchain does not expose `timegm()` even with `_GNU_SOURCE` defined (confirmed by a real compile failure while validating this plan: `'timegm' was not declared in this scope`). Use a self-contained integer date calculation instead — it behaves identically on the host and on-device.

```cpp
// firmware/src/iso8601.cpp
#include "iso8601.h"

#include <cstdio>

namespace {

// Days since 1970-01-01 for a proleptic-Gregorian civil date. Pure integer
// arithmetic (no libc time functions) so it behaves identically on the
// ESP32 Arduino toolchain and in native host unit tests. Algorithm from
// https://howardhinnant.github.io/date_algorithms.html#days_from_civil
int64_t daysFromCivil(int64_t year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int64_t era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy =
        (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

}  // namespace

int64_t parseIso8601ToEpoch(const std::string& iso8601) {
    int year, month, day, hour, minute, second;
    int fields = sscanf(iso8601.c_str(), "%d-%d-%dT%d:%d:%dZ", &year, &month,
                         &day, &hour, &minute, &second);
    if (fields != 6) {
        return -1;
    }

    int64_t days = daysFromCivil(year, static_cast<unsigned>(month),
                                  static_cast<unsigned>(day));
    return days * 86400 + hour * 3600 + minute * 60 + second;
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_iso8601`

Expected:
```
test/test_iso8601/test_iso8601.cpp:19: test_parses_valid_utc_timestamp	[PASSED]
test/test_iso8601/test_iso8601.cpp:20: test_returns_negative_one_on_malformed_input	[PASSED]
---------------- native:test_iso8601 [PASSED] ----------------
```

- [ ] **Step 6: Commit**

```bash
git add firmware/src/iso8601.h firmware/src/iso8601.cpp firmware/test/test_iso8601/test_iso8601.cpp
git commit -m "feat: add ISO-8601 to epoch parsing"
```

---

### Task 3: ETA Minutes Formatting

**Files:**
- Create: `firmware/src/eta_format.h`
- Create: `firmware/src/eta_format.cpp`
- Test: `firmware/test/test_eta_format/test_eta_format.cpp`

**Interfaces:**
- Consumes: nothing (takes raw epoch seconds as plain `int64_t`, no dependency on Task 2's header).
- Produces: `std::string formatEtaMinutes(int64_t targetEpoch, int64_t nowEpoch)` — `"--"` if `targetEpoch < 0`, `"Due"` if the rounded-to-nearest-minute difference is `<= 0`, `"60+"` if `> 60`, else `"<N>m"`. Used by Task 8 (`display.cpp`).

- [ ] **Step 1: Write the failing test**

```cpp
// firmware/test/test_eta_format/test_eta_format.cpp
#include <unity.h>

#include "eta_format.h"

void test_missing_eta_shows_placeholder() {
    TEST_ASSERT_EQUAL_STRING("--", formatEtaMinutes(-1, 1000).c_str());
}

void test_rounds_to_nearest_minute() {
    TEST_ASSERT_EQUAL_STRING("2m", formatEtaMinutes(1090, 1000).c_str());
}

void test_under_thirty_seconds_is_due() {
    TEST_ASSERT_EQUAL_STRING("Due", formatEtaMinutes(1010, 1000).c_str());
}

void test_past_arrival_is_due() {
    TEST_ASSERT_EQUAL_STRING("Due", formatEtaMinutes(995, 1000).c_str());
}

void test_over_an_hour_caps_at_sixty_plus() {
    TEST_ASSERT_EQUAL_STRING("60+", formatEtaMinutes(4660, 1000).c_str());
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_missing_eta_shows_placeholder);
    RUN_TEST(test_rounds_to_nearest_minute);
    RUN_TEST(test_under_thirty_seconds_is_due);
    RUN_TEST(test_past_arrival_is_due);
    RUN_TEST(test_over_an_hour_caps_at_sixty_plus);
    return UNITY_END();
}
```

- [ ] **Step 2: Create the header**

```cpp
// firmware/src/eta_format.h
#pragma once
#include <cstdint>
#include <string>

std::string formatEtaMinutes(int64_t targetEpoch, int64_t nowEpoch);
```

- [ ] **Step 3: Run the test to verify it fails**

Run: `pio test -e native -f test_eta_format`

Expected: `undefined symbols ... formatEtaMinutes ... [ERRORED]`

- [ ] **Step 4: Implement `formatEtaMinutes`**

```cpp
// firmware/src/eta_format.cpp
#include "eta_format.h"

std::string formatEtaMinutes(int64_t targetEpoch, int64_t nowEpoch) {
    if (targetEpoch < 0) {
        return "--";
    }

    int64_t diffSeconds = targetEpoch - nowEpoch;
    int64_t minutes = (diffSeconds >= 0) ? (diffSeconds + 30) / 60
                                          : -((-diffSeconds + 30) / 60);

    if (minutes <= 0) {
        return "Due";
    }
    if (minutes > 60) {
        return "60+";
    }
    return std::to_string(minutes) + "m";
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_eta_format`

Expected: all 5 assertions `[PASSED]`, suite `[PASSED]`.

- [ ] **Step 6: Commit**

```bash
git add firmware/src/eta_format.h firmware/src/eta_format.cpp firmware/test/test_eta_format/test_eta_format.cpp
git commit -m "feat: add ETA-to-minutes display formatting"
```

---

### Task 4: Bus Arrival JSON Parsing

**Files:**
- Create: `firmware/src/arrival_parser.h`
- Create: `firmware/src/arrival_parser.cpp`
- Test: `firmware/test/test_arrival_parser/test_arrival_parser.cpp`

**Interfaces:**
- Consumes: `int64_t parseIso8601ToEpoch(const std::string&)` from Task 2 (`iso8601.h`).
- Produces: `struct BusArrivalTimes { int64_t eta1Epoch, eta2Epoch, eta3Epoch; }` (each defaults to `-1`), `struct BusService { std::string serviceNo; BusArrivalTimes times; }`, `struct ParsedBusStop { bool valid; std::string busStopCode; std::vector<BusService> services; }`, `ParsedBusStop parseBusArrivalResponse(const std::string& json, const std::string& expectedStopCode)`, and `std::vector<BusService> selectDisplayServices(const std::vector<BusService>& services, size_t maxCount)`. Used by Task 7 (`bus_api_client` call site) and Task 8/10 (`display`/`main`).

- [ ] **Step 1: Write the failing test**

```cpp
// firmware/test/test_arrival_parser/test_arrival_parser.cpp
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
    RUN_TEST(test_select_display_services_caps_at_six);
    return UNITY_END();
}
```

- [ ] **Step 2: Create the header**

```cpp
// firmware/src/arrival_parser.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct BusArrivalTimes {
    int64_t eta1Epoch = -1;
    int64_t eta2Epoch = -1;
    int64_t eta3Epoch = -1;
};

struct BusService {
    std::string serviceNo;
    BusArrivalTimes times;
};

struct ParsedBusStop {
    bool valid = false;
    std::string busStopCode;
    std::vector<BusService> services;
};

ParsedBusStop parseBusArrivalResponse(const std::string& json,
                                       const std::string& expectedStopCode);

std::vector<BusService> selectDisplayServices(
    const std::vector<BusService>& services, size_t maxCount);
```

- [ ] **Step 3: Run the test to verify it fails**

Run: `pio test -e native -f test_arrival_parser`

Expected: `undefined symbols ... parseBusArrivalResponse ... selectDisplayServices ... [ERRORED]`

- [ ] **Step 4: Implement the parser**

```cpp
// firmware/src/arrival_parser.cpp
#include "arrival_parser.h"

#include <ArduinoJson.h>

#include "iso8601.h"

namespace {

int64_t readEta(JsonVariantConst nextBus) {
    if (nextBus.isNull()) {
        return -1;
    }
    const char* iso = nextBus["EstimatedArrival"] | "";
    if (iso[0] == '\0') {
        return -1;
    }
    return parseIso8601ToEpoch(iso);
}

}  // namespace

ParsedBusStop parseBusArrivalResponse(const std::string& json,
                                       const std::string& expectedStopCode) {
    ParsedBusStop result;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        return result;
    }

    for (JsonObjectConst stop : doc["busStops"].as<JsonArrayConst>()) {
        const char* code = stop["BusStopCode"] | "";
        if (expectedStopCode != code) {
            continue;
        }

        result.busStopCode = code;
        for (JsonObjectConst service : stop["Services"].as<JsonArrayConst>()) {
            BusService svc;
            svc.serviceNo = service["ServiceNo"] | "";
            svc.times.eta1Epoch = readEta(service["NextBus"]);
            svc.times.eta2Epoch = readEta(service["NextBus2"]);
            svc.times.eta3Epoch = readEta(service["NextBus3"]);
            result.services.push_back(svc);
        }
        result.valid = true;
        break;
    }

    return result;
}

std::vector<BusService> selectDisplayServices(
    const std::vector<BusService>& services, size_t maxCount) {
    std::vector<BusService> selected;
    for (size_t i = 0; i < services.size() && i < maxCount; ++i) {
        selected.push_back(services[i]);
    }
    return selected;
}
```

`parseBusArrivalResponse` takes `expectedStopCode` because a single `busStops` response could in principle contain entries the caller didn't ask about; matching by code (rather than always taking index 0) keeps `main.cpp` correct even if that assumption ever changes. It does **not** reorder or prioritize services — it returns them in the order the API returned them, and `selectDisplayServices` simply truncates to the first `maxCount`. If the API's ordering ever stops being "most relevant first," revisit `selectDisplayServices`.

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_arrival_parser`

Expected: all 5 tests `[PASSED]`.

- [ ] **Step 6: Run the full native suite**

Run: `pio test -e native`

Expected:
```
================= 12 test cases: 12 succeeded in 00:00:0X.XXX =================
```

- [ ] **Step 7: Commit**

```bash
git add firmware/src/arrival_parser.h firmware/src/arrival_parser.cpp firmware/test/test_arrival_parser/test_arrival_parser.cpp
git commit -m "feat: add BusArrival JSON parsing and 6-service selection"
```

---

### Task 5: WiFi Captive Portal Provisioning

**Files:**
- Modify: `firmware/src/main.cpp`

**Interfaces:**
- Consumes: `WiFiManager` (tzapu library, pinned in Task 1's `platformio.ini`).
- Produces: on-device WiFi setup so every later task can assume `WiFi.status() == WL_CONNECTED` after `setup()` returns (or the device has restarted to retry).

- [ ] **Step 1: Replace `firmware/src/main.cpp`'s boot smoke test with WiFi provisioning**

```cpp
// firmware/src/main.cpp
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>

namespace {

void showStatus(const char* line1, const char* line2 = "") {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(4, 4);
    M5.Display.println(line1);
    if (line2[0] != '\0') {
        M5.Display.println(line2);
    }
}

void onEnterConfigPortal(WiFiManager* wm) {
    showStatus("Connect WiFi to:", "BusAuntyDisplay-Setup");
}

}  // namespace

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    WiFiManager wm;
    wm.setAPCallback(onEnterConfigPortal);
    wm.setConfigPortalTimeout(180);
    showStatus("Connecting WiFi...");
    if (!wm.autoConnect("BusAuntyDisplay-Setup")) {
        showStatus("WiFi setup timed out.", "Restarting...");
        delay(3000);
        ESP.restart();
    }

    showStatus("WiFi connected:", WiFi.localIP().toString().c_str());
}

void loop() {
    M5.update();
}
```

- [ ] **Step 2: Build**

Run: `pio run -e sticks3`

Expected: `[SUCCESS]`.

- [ ] **Step 3: Manual on-device test — first-time provisioning**

1. Flash and open the serial monitor: `pio run -e sticks3 -t upload -t monitor`
2. On first boot (no saved WiFi credentials), the screen shows "Connecting WiFi..." then "Connect WiFi to: BusAuntyDisplay-Setup".
3. On a phone or laptop, join the open WiFi network named `BusAuntyDisplay-Setup`.
4. A captive-portal page should open automatically (or browse to `192.168.4.1`). Choose "Configure WiFi", select your real network, enter its password, and save.
5. The device screen should change to "WiFi connected:" followed by an IP address (e.g. `192.168.1.42`).

- [ ] **Step 4: Manual on-device test — reconnect on reboot**

Power-cycle the device (unplug/replug USB-C) without pressing any reset-portal button.

Expected: it reconnects to the already-configured network automatically and shows "WiFi connected: <ip>" within a few seconds, without re-entering the captive portal.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat: add WiFi captive portal provisioning"
```

---

### Task 6: NTP Time Sync

**Files:**
- Modify: `firmware/src/main.cpp`

**Interfaces:**
- Consumes: WiFi connectivity from Task 5.
- Produces: device system clock synced to UTC via `time(nullptr)`, which Task 7/10's ETA math depends on (arrival timestamps from the API are UTC `Z`-suffixed, so no timezone offset is applied — `configTime(0, 0, ...)`).

- [ ] **Step 1: Add `syncTime()` and call it after WiFi connects**

```cpp
// firmware/src/main.cpp
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

namespace {

void showStatus(const char* line1, const char* line2 = "") {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(4, 4);
    M5.Display.println(line1);
    if (line2[0] != '\0') {
        M5.Display.println(line2);
    }
}

void onEnterConfigPortal(WiFiManager* wm) {
    showStatus("Connect WiFi to:", "BusAuntyDisplay-Setup");
}

void syncTime() {
    showStatus("Syncing time...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    uint32_t startedAt = millis();
    while (now < 1700000000 && millis() - startedAt < 15000) {
        delay(250);
        now = time(nullptr);
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    WiFiManager wm;
    wm.setAPCallback(onEnterConfigPortal);
    wm.setConfigPortalTimeout(180);
    showStatus("Connecting WiFi...");
    if (!wm.autoConnect("BusAuntyDisplay-Setup")) {
        showStatus("WiFi setup timed out.", "Restarting...");
        delay(3000);
        ESP.restart();
    }

    syncTime();

    time_t now = time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmtime(&now));
    showStatus("Time synced (UTC):", buf);
}

void loop() {
    M5.update();
}
```

The `now < 1700000000` check (2023-11-14 UTC) is a sanity floor: before NTP succeeds, `time(nullptr)` reads roughly 0 (epoch), so this loop keeps retrying for up to 15s rather than immediately treating an un-synced clock as "synced".

- [ ] **Step 2: Build**

Run: `pio run -e sticks3`

Expected: `[SUCCESS]`.

- [ ] **Step 3: Manual on-device test**

Flash and monitor: `pio run -e sticks3 -t upload -t monitor`. After WiFi connects, the screen should show "Time synced (UTC):" followed by a date/time line.

Verify it's correct: run `date -u` on your computer and confirm the displayed UTC time is within a minute or two of that (the device doesn't display seconds-level precision on screen, but the serial log does — check `Serial` output isn't needed here since the display already proves it, but you can cross-check by temporarily adding `Serial.println(buf);` next to the `showStatus` call if you want a serial-log copy).

- [ ] **Step 4: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat: sync device clock to UTC via NTP"
```

---

### Task 7: HTTPS Bus Arrival API Client

**Files:**
- Create: `firmware/src/certs.h`
- Create: `firmware/src/bus_api_client.h`
- Create: `firmware/src/bus_api_client.cpp`
- Modify: `firmware/src/main.cpp`

**Interfaces:**
- Consumes: WiFi connectivity from Task 5.
- Produces: `struct FetchResult { bool ok; int httpStatus; std::string body; }` and `FetchResult fetchBusArrival(const std::string& busStopCode)`. Used by Task 10 (`main.cpp`'s poll loop).

- [ ] **Step 1: Create `firmware/src/certs.h` with the pinned root CA**

This is the "GTS Root R4" certificate (Google Trust Services) that signs `api.busaunty.com`'s certificate chain, extracted directly from the live server on 2026-09-02:

```bash
echo | openssl s_client -connect api.busaunty.com:443 -servername api.busaunty.com -showcerts 2>/dev/null | openssl x509 -noout -issuer -subject -dates
```

confirms `issuer= /C=US/O=Google Trust Services/CN=WE1`, `subject= /CN=busaunty.com`, valid through `Oct 19 13:44:59 2026 GMT` (the leaf cert; the root below is valid until 2028).

```cpp
// firmware/src/certs.h
#pragma once

// Google Trust Services "GTS Root R4", the CA that signs api.busaunty.com's
// certificate chain as of 2026-09-02 (verified via `openssl s_client
// -connect api.busaunty.com:443 -showcerts`). Expires 2028-01-28 -- if
// api.busaunty.com's certificate chain changes before then, HTTPS requests
// will fail with a TLS handshake error until this is updated.
static const char kGtsRootR4Pem[] = R"PEM(
-----BEGIN CERTIFICATE-----
MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX
MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE
CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIzMTEx
NTAzNDMyMVoXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT
GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFI0
MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE83Rzp2iLYK5DuDXFgTB7S0md+8Fhzube
Rr1r1WEYNa5A3XP3iZEwWus87oV8okB2O6nGuEfYKueSkWpz6bFyOZ8pn6KY019e
WIZlD6GEZQbR3IvJx3PIjGov5cSr0R2Ko4H/MIH8MA4GA1UdDwEB/wQEAwIBhjAd
BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDwYDVR0TAQH/BAUwAwEB/zAd
BgNVHQ4EFgQUgEzW63T/STaj1dj8tT7FavCUHYwwHwYDVR0jBBgwFoAUYHtmGkUN
l8qJUC99BM00qP/8/UswNgYIKwYBBQUHAQEEKjAoMCYGCCsGAQUFBzAChhpodHRw
Oi8vaS5wa2kuZ29vZy9nc3IxLmNydDAtBgNVHR8EJjAkMCKgIKAehhxodHRwOi8v
Yy5wa2kuZ29vZy9yL2dzcjEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqG
SIb3DQEBCwUAA4IBAQAYQrsPBtYDh5bjP2OBDwmkoWhIDDkic574y04tfzHpn+cJ
odI2D4SseesQ6bDrarZ7C30ddLibZatoKiws3UL9xnELz4ct92vID24FfVbiI1hY
+SW6FoVHkNeWIP0GCbaM4C6uVdF5dTUsMVs/ZbzNnIdCp5Gxmx5ejvEau8otR/Cs
kGN+hr/W5GvT1tMBjgWKZ1i4//emhA1JG1BbPzoLJQvyEotc03lXjTaCzv8mEbep
8RqZ7a2CPsgRbuvTPBwcOMBBmuFeU88+FSBX6+7iP0il8b4Z0QFqIwwMHfs/L6K1
vepuoxtGzi4CZ68zJpiq1UvSqTbFJjtbD4seiMHl
-----END CERTIFICATE-----
)PEM";
```

- [ ] **Step 2: Create `firmware/src/bus_api_client.h`**

```cpp
// firmware/src/bus_api_client.h
#pragma once
#include <string>

struct FetchResult {
    bool ok = false;
    int httpStatus = 0;
    std::string body;
};

FetchResult fetchBusArrival(const std::string& busStopCode);
```

- [ ] **Step 3: Implement `firmware/src/bus_api_client.cpp`**

```cpp
// firmware/src/bus_api_client.cpp
#include "bus_api_client.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "certs.h"

FetchResult fetchBusArrival(const std::string& busStopCode) {
    FetchResult result;

    WiFiClientSecure client;
    client.setCACert(kGtsRootR4Pem);

    HTTPClient http;
    http.setTimeout(8000);

    std::string url =
        "https://api.busaunty.com/api/v1/BusArrival?BusStopCode=" + busStopCode;
    if (!http.begin(client, url.c_str())) {
        return result;
    }

    result.httpStatus = http.GET();
    if (result.httpStatus == HTTP_CODE_OK) {
        result.ok = true;
        result.body = http.getString().c_str();
    }

    http.end();
    return result;
}
```

- [ ] **Step 4: Temporarily wire a fetch into `main.cpp` for manual verification**

Add the include and call at the end of `setup()`, after `syncTime()`:

```cpp
// firmware/src/main.cpp
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include <string>

#include "bus_api_client.h"

namespace {

void showStatus(const char* line1, const char* line2 = "") {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(4, 4);
    M5.Display.println(line1);
    if (line2[0] != '\0') {
        M5.Display.println(line2);
    }
}

void onEnterConfigPortal(WiFiManager* wm) {
    showStatus("Connect WiFi to:", "BusAuntyDisplay-Setup");
}

void syncTime() {
    showStatus("Syncing time...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    uint32_t startedAt = millis();
    while (now < 1700000000 && millis() - startedAt < 15000) {
        delay(250);
        now = time(nullptr);
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    WiFiManager wm;
    wm.setAPCallback(onEnterConfigPortal);
    wm.setConfigPortalTimeout(180);
    showStatus("Connecting WiFi...");
    if (!wm.autoConnect("BusAuntyDisplay-Setup")) {
        showStatus("WiFi setup timed out.", "Restarting...");
        delay(3000);
        ESP.restart();
    }

    syncTime();

    FetchResult fetch = fetchBusArrival("53389");
    Serial.printf("HTTP status: %d\n", fetch.httpStatus);
    Serial.println(fetch.body.c_str());
    showStatus("Fetch HTTP status:", std::to_string(fetch.httpStatus).c_str());
}

void loop() {
    M5.update();
}
```

- [ ] **Step 5: Build**

Run: `pio run -e sticks3`

Expected: `[SUCCESS]`.

- [ ] **Step 6: Manual on-device test against the real API**

1. Flash and monitor: `pio run -e sticks3 -t upload -t monitor`
2. Expected screen: "Fetch HTTP status: 200".
3. In the serial monitor, expected: `HTTP status: 200` followed by a JSON body starting with `{"busStops":[{"BusStopCode":"53389", ...`.
4. Cross-check against a direct request from your computer:
   ```bash
   curl -s "https://api.busaunty.com/api/v1/BusArrival?BusStopCode=53389" | head -c 500
   ```
   The `ServiceNo` values and general shape should match what the device printed (arrival times will differ slightly between the two calls since buses keep moving, that's expected).

- [ ] **Step 7: Commit**

```bash
git add firmware/src/certs.h firmware/src/bus_api_client.h firmware/src/bus_api_client.cpp firmware/src/main.cpp
git commit -m "feat: add HTTPS BusArrival API client with pinned root CA"
```

---

### Task 8: Arrival Display Rendering

**Files:**
- Create: `firmware/src/display.h`
- Create: `firmware/src/display.cpp`
- Modify: `firmware/src/main.cpp`

**Interfaces:**
- Consumes: `BusService`/`BusArrivalTimes` from Task 4 (`arrival_parser.h`), `formatEtaMinutes` from Task 3 (`eta_format.h`).
- Produces: `void displaySetup()`, `void displayShowStatus(const std::string& message)`, `void displayShowArrivals(const std::string& busStopCode, const std::vector<BusService>& services, int64_t nowEpoch, size_t currentStopIndex, size_t totalStops)`. Used by Task 9/10 (`main.cpp`), replacing the raw `showStatus()` helper from Tasks 5-7.

- [ ] **Step 1: Create `firmware/src/display.h`**

```cpp
// firmware/src/display.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "arrival_parser.h"

void displaySetup();
void displayShowStatus(const std::string& message);
void displayShowArrivals(const std::string& busStopCode,
                          const std::vector<BusService>& services,
                          int64_t nowEpoch, size_t currentStopIndex,
                          size_t totalStops);
```

- [ ] **Step 2: Implement `firmware/src/display.cpp`**

Layout: a sprite-buffered (flicker-free) 240x135 canvas using M5GFX's built-in font 2 (16px tall). Row 0 is the header (stop code + "(current/total)"); rows 1-6 are up to 6 services, each 16px tall, service number left-aligned at x=4, the three ETAs right-aligned ending at x=150/195/236. 16 (header) + 6*16 (rows) = 112px, well inside the 135px screen height.

```cpp
// firmware/src/display.cpp
#include "display.h"

#include <M5Unified.h>

#include "eta_format.h"

namespace {

M5Canvas canvas(&M5.Display);

constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;
constexpr int kRowHeight = 16;
constexpr int kFirstRowY = 16;
constexpr int kServiceColX = 4;
constexpr int kEtaColX[3] = {150, 195, 236};

}  // namespace

void displaySetup() {
    M5.Display.setRotation(1);
    canvas.setColorDepth(8);
    canvas.createSprite(kScreenWidth, kScreenHeight);
    canvas.setTextFont(2);
    canvas.setTextSize(1);
}

void displayShowStatus(const std::string& message) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextDatum(middle_center);
    canvas.drawString(message.c_str(), kScreenWidth / 2, kScreenHeight / 2);
    canvas.pushSprite(0, 0);
}

void displayShowArrivals(const std::string& busStopCode,
                          const std::vector<BusService>& services,
                          int64_t nowEpoch, size_t currentStopIndex,
                          size_t totalStops) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);

    canvas.setTextDatum(top_left);
    std::string header = busStopCode + " (" +
                          std::to_string(currentStopIndex + 1) + "/" +
                          std::to_string(totalStops) + ")";
    canvas.drawString(header.c_str(), kServiceColX, 0);

    for (size_t i = 0; i < services.size() && i < 6; ++i) {
        int y = kFirstRowY + static_cast<int>(i) * kRowHeight;
        const BusService& svc = services[i];

        canvas.setTextDatum(top_left);
        canvas.drawString(svc.serviceNo.c_str(), kServiceColX, y);

        canvas.setTextDatum(top_right);
        canvas.drawString(
            formatEtaMinutes(svc.times.eta1Epoch, nowEpoch).c_str(),
            kEtaColX[0], y);
        canvas.drawString(
            formatEtaMinutes(svc.times.eta2Epoch, nowEpoch).c_str(),
            kEtaColX[1], y);
        canvas.drawString(
            formatEtaMinutes(svc.times.eta3Epoch, nowEpoch).c_str(),
            kEtaColX[2], y);
    }

    canvas.pushSprite(0, 0);
}
```

- [ ] **Step 3: Wire `main.cpp` to use the new display module, with a hardcoded demo grid for visual verification**

This replaces the raw `showStatus()` helper and the Task 7 fetch-verification block with calls into `display.h`, plus a temporary hardcoded 6-service demo (Task 9 will make this data real).

```cpp
// firmware/src/main.cpp
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include <string>
#include <vector>

#include "arrival_parser.h"
#include "display.h"

namespace {

void onEnterConfigPortal(WiFiManager* wm) {
    displayShowStatus("Connect WiFi to:\nBusAuntyDisplay-Setup");
}

void syncTime() {
    displayShowStatus("Syncing time...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    uint32_t startedAt = millis();
    while (now < 1700000000 && millis() - startedAt < 15000) {
        delay(250);
        now = time(nullptr);
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    displaySetup();

    WiFiManager wm;
    wm.setAPCallback(onEnterConfigPortal);
    wm.setConfigPortalTimeout(180);
    displayShowStatus("Connecting WiFi...");
    if (!wm.autoConnect("BusAuntyDisplay-Setup")) {
        displayShowStatus("WiFi setup timed out.\nRestarting...");
        delay(3000);
        ESP.restart();
    }

    syncTime();

    std::vector<BusService> demoServices;
    const char* demoNumbers[] = {"12", "147", "36", "980", "5", "88"};
    for (int i = 0; i < 6; ++i) {
        BusService svc;
        svc.serviceNo = demoNumbers[i];
        svc.times.eta1Epoch = time(nullptr) + (i + 1) * 90;
        svc.times.eta2Epoch = time(nullptr) + (i + 1) * 300;
        svc.times.eta3Epoch = time(nullptr) + (i + 1) * 600;
        demoServices.push_back(svc);
    }
    displayShowArrivals("53389", demoServices, time(nullptr), 0, 1);
}

void loop() {
    M5.update();
}
```

Note: `bus_api_client.h`/`fetchBusArrival` is no longer referenced here — that's expected, Task 10 reintroduces it as part of the real poll loop.

- [ ] **Step 4: Build**

Run: `pio run -e sticks3`

Expected: `[SUCCESS]`.

- [ ] **Step 5: Manual on-device visual check**

Flash: `pio run -e sticks3 -t upload`. After WiFi connects and time syncs, the screen should show:
- Top-left: `53389 (1/1)`
- 6 rows below, each with a service number on the left (`12`, `147`, `36`, `980`, `5`, `88`) and three right-aligned ETA columns per row (increasing values like `2m`/`5m`/`10m` on the first row, larger on later rows since the demo data spaces them out).
- All text fits on-screen with no clipping or wraparound; if any column overlaps another, the `kEtaColX`/`kServiceColX` constants in `display.cpp` need adjusting before continuing.

- [ ] **Step 6: Commit**

```bash
git add firmware/src/display.h firmware/src/display.cpp firmware/src/main.cpp
git commit -m "feat: add 6-service arrival board rendering"
```

---

### Task 9: Multi-Stop Configuration and Button Cycling

**Files:**
- Create: `firmware/src/bus_stops.h`
- Modify: `firmware/src/main.cpp`

**Interfaces:**
- Consumes: `displayShowArrivals` from Task 8.
- Produces: `constexpr std::array<const char*, N> kBusStopCodes` (the configured stop list) and `M5.BtnA`-driven cycling through it, consumed by Task 10's poll loop.

- [ ] **Step 1: Create `firmware/src/bus_stops.h`**

Seeded with the stop code from your example (`53389`) plus `67379` (one of the multi-stop example codes already used in `docs/README.md`), purely so this task has two real, distinct codes to prove cycling works end to end. Edit this array to whatever stops you actually want to see.

```cpp
// firmware/src/bus_stops.h
#pragma once
#include <array>

// Starter set: replace with the bus stop codes you actually want to see.
inline constexpr std::array<const char*, 2> kBusStopCodes = {
    "53389",
    "67379",
};
```

- [ ] **Step 2: Wire button cycling into `main.cpp`, driving the same demo grid by stop index**

```cpp
// firmware/src/main.cpp
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include <string>
#include <vector>

#include "arrival_parser.h"
#include "bus_stops.h"
#include "display.h"

namespace {

size_t currentStopIndex = 0;

void onEnterConfigPortal(WiFiManager* wm) {
    displayShowStatus("Connect WiFi to:\nBusAuntyDisplay-Setup");
}

void syncTime() {
    displayShowStatus("Syncing time...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    uint32_t startedAt = millis();
    while (now < 1700000000 && millis() - startedAt < 15000) {
        delay(250);
        now = time(nullptr);
    }
}

void renderDemo(size_t stopIndex) {
    std::vector<BusService> demoServices;
    const char* demoNumbers[] = {"12", "147", "36", "980", "5", "88"};
    for (int i = 0; i < 6; ++i) {
        BusService svc;
        svc.serviceNo = demoNumbers[i];
        svc.times.eta1Epoch = time(nullptr) + (i + 1) * 90;
        svc.times.eta2Epoch = time(nullptr) + (i + 1) * 300;
        svc.times.eta3Epoch = time(nullptr) + (i + 1) * 600;
        demoServices.push_back(svc);
    }
    displayShowArrivals(kBusStopCodes[stopIndex], demoServices, time(nullptr),
                         stopIndex, kBusStopCodes.size());
}

}  // namespace

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    displaySetup();

    WiFiManager wm;
    wm.setAPCallback(onEnterConfigPortal);
    wm.setConfigPortalTimeout(180);
    displayShowStatus("Connecting WiFi...");
    if (!wm.autoConnect("BusAuntyDisplay-Setup")) {
        displayShowStatus("WiFi setup timed out.\nRestarting...");
        delay(3000);
        ESP.restart();
    }

    syncTime();
    renderDemo(currentStopIndex);
}

void loop() {
    M5.update();
    if (M5.BtnA.wasPressed()) {
        currentStopIndex = (currentStopIndex + 1) % kBusStopCodes.size();
        renderDemo(currentStopIndex);
    }
}
```

- [ ] **Step 3: Build**

Run: `pio run -e sticks3`

Expected: `[SUCCESS]`.

- [ ] **Step 4: Manual on-device test**

Flash: `pio run -e sticks3 -t upload`. After boot, the header reads `53389 (1/2)`. Press KEY1 (the main front button): the header should change to `67379 (2/2)`. Press it again: it should wrap back to `53389 (1/2)`.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/bus_stops.h firmware/src/main.cpp
git commit -m "feat: add multi-stop config and button-driven cycling"
```

---

### Task 10: End-to-End Polling Loop

**Files:**
- Modify: `firmware/src/main.cpp`

**Interfaces:**
- Consumes: `fetchBusArrival`/`FetchResult` (Task 7), `parseBusArrivalResponse`/`selectDisplayServices` (Task 4), `displayShowStatus`/`displayShowArrivals` (Task 8), `kBusStopCodes` (Task 9).
- Produces: the finished firmware behavior — this is the last task that touches `main.cpp`.

- [ ] **Step 1: Replace the demo rendering with real fetch + parse + render, on a 30s timer, with WiFi-loss and API-error handling**

```cpp
// firmware/src/main.cpp
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include <string>

#include "arrival_parser.h"
#include "bus_api_client.h"
#include "bus_stops.h"
#include "display.h"

namespace {

constexpr uint32_t kPollIntervalMs = 30000;

size_t currentStopIndex = 0;
uint32_t lastPollMillis = 0;
bool needsImmediateFetch = true;

void onEnterConfigPortal(WiFiManager* wm) {
    displayShowStatus("Connect WiFi to:\nBusAuntyDisplay-Setup");
}

void syncTime() {
    displayShowStatus("Syncing time...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    uint32_t startedAt = millis();
    while (now < 1700000000 && millis() - startedAt < 15000) {
        delay(250);
        now = time(nullptr);
    }
}

void pollAndRender() {
    const char* code = kBusStopCodes[currentStopIndex];
    displayShowStatus(std::string("Loading ") + code + "...");

    FetchResult fetch = fetchBusArrival(code);
    if (!fetch.ok) {
        displayShowStatus(fetch.httpStatus == 404
                               ? std::string("No data for ") + code
                               : std::string("Fetch failed (") +
                                     std::to_string(fetch.httpStatus) + ")");
        return;
    }

    ParsedBusStop parsed = parseBusArrivalResponse(fetch.body, code);
    if (!parsed.valid) {
        displayShowStatus(std::string("Bad response for ") + code);
        return;
    }

    std::vector<BusService> shown = selectDisplayServices(parsed.services, 6);
    displayShowArrivals(parsed.busStopCode, shown, time(nullptr),
                         currentStopIndex, kBusStopCodes.size());
}

}  // namespace

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    displaySetup();

    WiFiManager wm;
    wm.setAPCallback(onEnterConfigPortal);
    wm.setConfigPortalTimeout(180);
    displayShowStatus("Connecting WiFi...");
    if (!wm.autoConnect("BusAuntyDisplay-Setup")) {
        displayShowStatus("WiFi setup timed out.\nRestarting...");
        delay(3000);
        ESP.restart();
    }

    syncTime();
}

void loop() {
    M5.update();

    if (M5.BtnA.wasPressed()) {
        currentStopIndex = (currentStopIndex + 1) % kBusStopCodes.size();
        needsImmediateFetch = true;
    }

    if (WiFi.status() != WL_CONNECTED) {
        displayShowStatus("WiFi lost, reconnecting...");
        WiFi.reconnect();
        delay(1000);
        return;
    }

    uint32_t now = millis();
    if (needsImmediateFetch || now - lastPollMillis >= kPollIntervalMs) {
        pollAndRender();
        lastPollMillis = now;
        needsImmediateFetch = false;
    }
}
```

- [ ] **Step 2: Build**

Run: `pio run -e sticks3`

Expected: `[SUCCESS]` (this exact file was compiled successfully while validating this plan — Flash 44.6%, RAM 16.5% used).

- [ ] **Step 3: Manual end-to-end test against the real API**

1. Flash: `pio run -e sticks3 -t upload -t monitor`.
2. After WiFi connects and time syncs, within a few seconds the screen shows `53389 (1/2)` with up to 6 real service numbers and ETAs.
3. Compare against a direct API call:
   ```bash
   curl -s "https://api.busaunty.com/api/v1/BusArrival?BusStopCode=53389" | python3 -m json.tool
   ```
   Confirm the service numbers shown on-screen match the first 6 `ServiceNo` entries in the response, and that the displayed minute counts are plausible given each `NextBus.EstimatedArrival` vs. the current time.
4. Press KEY1: the header should switch to `67379 (2/2)` and the grid should refresh with that stop's real data within about a second.
5. Wait 30+ seconds without pressing anything: the displayed minute counts should tick down (e.g. `5m` becomes `4m`), confirming the poll timer is running.
6. Turn off your WiFi router (or move the device out of range) briefly: the screen should show "WiFi lost, reconnecting..."; restore WiFi and confirm it recovers and resumes polling without a manual reset.
7. Temporarily query an obviously-invalid stop code to confirm error handling, e.g. edit `bus_stops.h` to `{"00000"}`, reflash, and confirm the screen shows "No data for 00000" (matching the API's documented 404-for-no-data behavior) rather than crashing or freezing. Revert `bus_stops.h` back to the real codes afterward.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat: wire full 30s poll loop with error handling"
```

---

### Task 11: Firmware README

**Files:**
- Create: `firmware/README.md`

**Interfaces:**
- Consumes: nothing (documentation only).
- Produces: nothing consumed by other tasks.

- [ ] **Step 1: Write `firmware/README.md`**

```markdown
# Bus Aunty Display Firmware

ESP32-S3 (M5Stack StickS3) firmware that polls the [Bus Timer API](../docs/README.md)
every 30 seconds and shows up to 6 bus services, with all 3 arrival times each,
for one or more configured bus stops.

## Hardware

- [M5Stack StickS3](https://docs.m5stack.com/en/core/StickS3) (ESP32-S3-PICO-1-N8R8,
  8MB flash, 8MB Octal PSRAM, 135x240 ST7789P3 display).
- USB-C cable for flashing and power.

## Prerequisites

- Python 3 and pip.
- PlatformIO Core: `pip install platformio` (adds a `pio` command; on macOS/Linux
  it's usually installed to `~/.local/bin` or `~/Library/Python/<ver>/bin` —
  add that to your `PATH`, or call it via its full path).

## Build, flash, and monitor

```bash
cd firmware
pio run -e sticks3                    # build only
pio run -e sticks3 -t upload           # build + flash over USB-C
pio run -e sticks3 -t upload -t monitor  # build + flash + open serial monitor
```

## Run the unit tests

The pure logic (ISO-8601 parsing, ETA formatting, JSON parsing) runs as
host-native unit tests — no device needed:

```bash
cd firmware
pio test -e native
```

Hardware-facing code (WiFi provisioning, HTTPS fetch, display rendering,
button input) isn't unit-testable this way; it's verified manually on-device
per the checklist in each task of
`../docs/superpowers/plans/2026-09-02-bus-arrival-display.md`.

## First-time WiFi setup

1. Flash the firmware and power on the device.
2. It opens an open WiFi access point named `BusAuntyDisplay-Setup`.
3. Connect to it from your phone or laptop; a captive-portal page should open
   automatically (or browse to `192.168.4.1`).
4. Choose "Configure WiFi", select your network, enter its password, save.
5. The device reconnects and starts polling. Credentials are stored on-device
   (not in source) and are reused automatically on every future boot.

To re-provision (e.g. new WiFi network), erase the device's flash and reflash,
or add a hold-button-to-reset-WiFi flow if you need one (not implemented here).

## Configuring bus stops

Edit the list in `src/bus_stops.h`:

```cpp
inline constexpr std::array<const char*, N> kBusStopCodes = {
    "53389",
    "67379",
    // add or remove codes here
};
```

Rebuild and reflash after changing it. Press the main front button (KEY1) on
the device to cycle between configured stops.

## Notes

- The TLS root certificate pinned in `src/certs.h` (Google Trust Services
  "GTS Root R4") expires 2028-01-28. If `api.busaunty.com`'s certificate
  chain changes before then, HTTPS requests will start failing with a TLS
  handshake error until `certs.h` is updated with the new chain's root CA.
- Arrival times are computed as "minutes from now" using the device's
  NTP-synced UTC clock — see `src/eta_format.cpp`.
```

- [ ] **Step 2: Commit**

```bash
git add firmware/README.md
git commit -m "docs: add firmware build, flash, and setup instructions"
```

---

## Unresolved Questions

- **Cert rotation:** the pinned `GTS Root R4` root (Task 7) expires 2028-01-28, and there's no auto-update path — if Cloudflare/Google rotate `api.busaunty.com`'s CA before then, the device will need a firmware update. Acceptable, or should the plan instead use `WiFiClientSecure::setInsecure()` (skips certificate validation) to sidestep rotation at the cost of losing TLS verification?
- **Seed bus stop list:** Task 9 seeds `bus_stops.h` with `53389` (your example) plus `67379` (an example code from the existing `docs/README.md`, used only to prove cycling works). What's the real list of stops you want configured before first flash?
- **Captive-portal security:** the WiFi setup AP (`BusAuntyDisplay-Setup`) is unauthenticated/open. Fine for home use — do you want a portal password (`wm.autoConnect(apName, apPassword)`) instead?
- **Clock-sync failure behavior:** if NTP sync fails after the 15s timeout (Task 6), the device proceeds anyway with an unsynced clock (epoch near 0). Correction from the final whole-branch review: this does **not** make every arrival show "Due" — the pinned root CA's certificate has a `notBefore` of 2023-11-15, so `WiFiClientSecure`'s TLS handshake fails the certificate-validity check against an unsynced (~1970) clock, and `fetchBusArrival` never returns a body at all. The screen would show "Fetch failed (-1)" indefinitely instead. That's arguably safer (it fails closed rather than displaying confidently-wrong ETAs) but is also a much more confusing failure mode to diagnose on real hardware than the original "stuck showing Due" theory suggested. Is a silent-looking "Fetch failed (-1)" loop acceptable, or should `syncTime()` show an explicit "clock not synced" message and retry indefinitely instead of falling through to WiFiManager/HTTPS calls that will silently fail?
- **Power/brightness management:** this plan runs the device always-on at full brightness, polling every 30s indefinitely on USB power. No sleep/dimming behavior was requested — is that fine, or do you want the screen to dim or the device to sleep overnight given the 250mAh battery if it's ever run unplugged?
