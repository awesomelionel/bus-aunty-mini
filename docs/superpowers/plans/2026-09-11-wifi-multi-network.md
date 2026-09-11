# Multi-Network WiFi Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace WiFiManager with an owned multi-network WiFi stack: a priority-ordered list of saved networks, a non-blocking connect/retry state machine, and a button-unlocked LAN config page at `http://busaunty.local`.

**Architecture:** Pure credential and policy logic lives in `src/core/` and is host-tested. NVS persistence, the radio state machine, and the HTTP config page sit in `src/storage/` and `src/net/` and hold no policy of their own. `main.cpp` stops blocking on WiFi at boot and instead ticks the state machine each loop.

**Tech Stack:** PlatformIO, Arduino, ESP32 core (`WiFi`, `WebServer`, `DNSServer`, `ESPmDNS`, `esp_wifi`), ArduinoJson v7 (arrivals only — WiFi credentials use the existing `\x1f`/`\x1e` blob encoding), Unity on `[env:native]`. `tzapu/WiFiManager` is removed.

**Spec:** `docs/superpowers/specs/2026-09-11-wifi-multi-network-design.md`

## Global Constraints

- No new `lib_deps`. Drop `tzapu/WiFiManager@^2.0.17` from `[esp32_base]`.
- Credentials persist in the existing `busaunty` NVS namespace under key `wifi`, encoded with `\x1f` field / `\x1e` record separators — same encoding as `serializeBusStops`, not JSON.
- SSID is stored byte-exact UTF-8 (1–32 bytes). Password is empty (open) or 8–63 chars. Rows containing `\x1e` or `\x1f` are **rejected**, never sanitized.
- Saved passwords are never rendered into HTML. A blank password field on edit means keep the existing one.
- Setup AP is WPA2; passphrase is the last 4 MAC octets as 8 hex digits, shown on screen.
- LAN config page is served only during a 5-minute unlock window opened by the config gesture.
- Vector order is priority; index 0 is tried first. Cap is `kMaxWifiNetworks = 5`.
- Fast path (last-good `WiFi.begin`, no scan) is used on wake only. Cold boot and network-list changes start with a scan. AP fallback is only for zero saved networks or an explicit request — never because nothing is in range.
- `enterSleep()` radio teardown (`disconnect` then `WIFI_OFF`) is left as-is.
- Follow existing comment voice and TDD: failing native test, then implementation, then commit per task.
- Host tests run from `firmware/`: `pio test -e native -f test_<name>`. Board builds: `pio run -e sticks3 -e feather_s3_revtft`.

## File map

| File | Role |
| --- | --- |
| `include/core/wifi_credentials.h` + `src/core/wifi_credentials.cpp` | `WifiNetwork`, validation, merge, serialize, AP password |
| `include/core/wifi_policy.h` + `src/core/wifi_policy.cpp` | Candidate pick, backoff, upgrade decision |
| `include/core/html_escape.h` + `src/core/html_escape.cpp` | Attribute-safe HTML escaping for the config page |
| `include/storage/wifi_store.h` + `src/storage/wifi_store.cpp` | NVS load/save + "key exists" for migration |
| `include/net/wifi_link.h` + `src/net/wifi_link.cpp` | Radio state machine, AP fallback, last-good, visible-scan |
| `include/net/config_server.h` + `src/net/config_server.cpp` | WebServer + mDNS + DNSServer + unlock gate |
| `include/net/wifi_portal.h` + `src/net/wifi_portal.cpp` | Deleted |
| `include/ui/display.h` + `src/ui/display.cpp` | AP password on setup screen; new config + offline screens |
| `src/main.cpp` | Wire the machine; drop blocking portal / reboot |
| `firmware/platformio.ini` | Drop WiFiManager |
| `README.md` | New setup flow + 2.4 GHz hotspot note |
| `test/test_wifi_credentials/` | Host tests |
| `test/test_wifi_policy/` | Host tests |
| `test/test_html_escape/` | Host tests |

---

### Task 1: Credential model

**Files:**
- Create: `firmware/include/core/wifi_credentials.h`
- Create: `firmware/src/core/wifi_credentials.cpp`
- Test: `firmware/test/test_wifi_credentials/test_wifi_credentials.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:

```cpp
constexpr size_t kMaxWifiNetworks = 5;
constexpr size_t kWifiSsidMinBytes = 1;
constexpr size_t kWifiSsidMaxBytes = 32;
constexpr size_t kWifiPasswordMinChars = 8;
constexpr size_t kWifiPasswordMaxChars = 63;

struct WifiNetwork {
    std::string ssid;
    std::string password;
};

bool validateWifiSsid(const std::string& ssid);
bool validateWifiPassword(const std::string& password);
std::vector<WifiNetwork> buildWifiNetworkList(
    const std::vector<WifiNetwork>& rows);
std::vector<WifiNetwork> mergeWifiPasswords(
    const std::vector<WifiNetwork>& existing,
    const std::vector<WifiNetwork>& incoming);
std::string serializeWifiNetworks(const std::vector<WifiNetwork>& networks);
std::vector<WifiNetwork> deserializeWifiNetworks(const std::string& blob);
std::string deriveApPassword(const uint8_t mac[6]);
```

`mergeWifiPasswords`: for each incoming row whose password is empty, copy the password from the first existing row with the same SSID. A new SSID with an empty password stays an open network. Then run the result through `buildWifiNetworkList`.

`deriveApPassword`: last 4 octets as uppercase hex, always 8 characters (valid WPA2). Example MAC `AA:BB:CC:DD:EE:FF` → `"CCDDEEFF"`.

- [ ] **Step 1: Write the failing tests**

```cpp
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
    std::vector<WifiNetwork> incoming = {{"Home", ""},
                                         {"Hotspot", ""}};
    std::vector<WifiNetwork> merged = mergeWifiPasswords(existing, incoming);
    TEST_ASSERT_EQUAL_UINT32(2, merged.size());
    TEST_ASSERT_EQUAL_STRING("secret12", merged[0].password.c_str());
    TEST_ASSERT_EQUAL_STRING("", merged[1].password.c_str());
}

void test_round_trips_utf8_ssid() {
    const std::string iphone = "Lionel\xe2\x80\x99s iPhone";
    std::vector<WifiNetwork> nets = {{iphone, "hotspot1"},
                                     {"Home", ""}};
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd firmware && pio test -e native -f test_wifi_credentials`

Expected: FAIL — `core/wifi_credentials.h` not found.

- [ ] **Step 3: Write the header and implementation**

Header comments follow the house voice (what and why, not narration). Implementation mirrors `bus_stop_config.cpp`: same separators, same deserialize-then-`build*` revalidation. `validateWifiSsid` / `validateWifiPassword` must not trim or strip — a leading/trailing space is a real character in an SSID.

`deserializeWifiNetworks("no-separator")` produces one row with a password-less SSID `"no-separator"`. `buildWifiNetworkList` then drops it because an SSID with no password field is treated as `{ssid: "no-separator", password: ""}` — wait: a missing field separator means the whole record is the SSID and the password is empty, which is a valid open network. That would make `test_corrupt_blob_yields_no_bad_networks` fail.

Fix the test's intent in the implementation contract: a record without a field separator is corrupt and dropped. Only records with a field separator are accepted (password may still be empty: `"Home\x1f"`).

```cpp
#include "core/wifi_credentials.h"

#include <cstdio>

namespace {

constexpr char kFieldSep = '\x1f';
constexpr char kRecordSep = '\x1e';

bool containsSeparator(const std::string& s) {
    return s.find(kFieldSep) != std::string::npos ||
           s.find(kRecordSep) != std::string::npos;
}

}  // namespace

bool validateWifiSsid(const std::string& ssid) {
    return ssid.size() >= kWifiSsidMinBytes &&
           ssid.size() <= kWifiSsidMaxBytes && !containsSeparator(ssid);
}

bool validateWifiPassword(const std::string& password) {
    if (containsSeparator(password)) {
        return false;
    }
    if (password.empty()) {
        return true;
    }
    return password.size() >= kWifiPasswordMinChars &&
           password.size() <= kWifiPasswordMaxChars;
}

std::vector<WifiNetwork> buildWifiNetworkList(
    const std::vector<WifiNetwork>& rows) {
    std::vector<WifiNetwork> networks;
    for (const WifiNetwork& row : rows) {
        if (networks.size() >= kMaxWifiNetworks) {
            break;
        }
        if (!validateWifiSsid(row.ssid) || !validateWifiPassword(row.password)) {
            continue;
        }
        networks.push_back(row);
    }
    return networks;
}

std::vector<WifiNetwork> mergeWifiPasswords(
    const std::vector<WifiNetwork>& existing,
    const std::vector<WifiNetwork>& incoming) {
    std::vector<WifiNetwork> rows = incoming;
    for (WifiNetwork& row : rows) {
        if (!row.password.empty()) {
            continue;
        }
        for (const WifiNetwork& prior : existing) {
            if (prior.ssid == row.ssid) {
                row.password = prior.password;
                break;
            }
        }
    }
    return buildWifiNetworkList(rows);
}

std::string serializeWifiNetworks(const std::vector<WifiNetwork>& networks) {
    std::string blob;
    for (size_t i = 0; i < networks.size(); ++i) {
        if (i > 0) {
            blob.push_back(kRecordSep);
        }
        blob += networks[i].ssid;
        blob.push_back(kFieldSep);
        blob += networks[i].password;
    }
    return blob;
}

std::vector<WifiNetwork> deserializeWifiNetworks(const std::string& blob) {
    std::vector<WifiNetwork> rows;
    if (blob.empty()) {
        return {};
    }
    size_t start = 0;
    while (start <= blob.size()) {
        size_t recordEnd = blob.find(kRecordSep, start);
        if (recordEnd == std::string::npos) {
            recordEnd = blob.size();
        }
        std::string record = blob.substr(start, recordEnd - start);
        size_t fieldEnd = record.find(kFieldSep);
        if (fieldEnd != std::string::npos) {
            WifiNetwork row;
            row.ssid = record.substr(0, fieldEnd);
            row.password = record.substr(fieldEnd + 1);
            rows.push_back(row);
        }
        if (recordEnd == blob.size()) {
            break;
        }
        start = recordEnd + 1;
    }
    return buildWifiNetworkList(rows);
}

std::string deriveApPassword(const uint8_t mac[6]) {
    char buf[9];
    std::snprintf(buf, sizeof(buf), "%02X%02X%02X%02X", mac[2], mac[3], mac[4],
                  mac[5]);
    return buf;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd firmware && pio test -e native -f test_wifi_credentials`

Expected: all 11 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add firmware/include/core/wifi_credentials.h \
        firmware/src/core/wifi_credentials.cpp \
        firmware/test/test_wifi_credentials/test_wifi_credentials.cpp
git commit -m "$(cat <<'EOF'
feat(firmware): add multi-network WiFi credential model

EOF
)"
```

---

### Task 2: Connection policy

**Files:**
- Create: `firmware/include/core/wifi_policy.h`
- Create: `firmware/src/core/wifi_policy.cpp`
- Test: `firmware/test/test_wifi_policy/test_wifi_policy.cpp`

**Interfaces:**
- Consumes: `WifiNetwork` from Task 1.
- Produces:

```cpp
constexpr uint32_t kWifiBackoffStepsMs[] = {5000, 15000, 60000};
constexpr uint32_t kWifiUpgradeIntervalMs = 300000;
constexpr uint32_t kWifiConnectTimeoutMs = 10000;
constexpr uint32_t kWifiUnlockWindowMs = 300000;

std::vector<size_t> selectWifiCandidates(
    const std::vector<WifiNetwork>& saved,
    const std::vector<std::string>& seen);

uint32_t wifiBackoffMs(uint32_t failedCycles);

bool shouldUpgradeWifi(const std::vector<WifiNetwork>& saved,
                       const std::string& currentSsid,
                       const std::vector<std::string>& seen);
```

`selectWifiCandidates` returns indices into `saved`, in saved order, for SSIDs that appear in `seen`. Each saved index appears at most once.

`wifiBackoffMs`: `failedCycles == 0` → 5000, `1` → 15000, `>= 2` → 60000.

`shouldUpgradeWifi`: false when `saved` is empty or `currentSsid == saved[0].ssid`. True when any earlier-than-current saved SSID is in `seen`. If `currentSsid` is not in `saved`, treat it as below every saved network (upgrade if any saved SSID is in `seen`).

- [ ] **Step 1: Write the failing tests**

```cpp
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
    std::vector<size_t> got =
        selectWifiCandidates(three(), {"Home", "Home"});
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
    TEST_ASSERT_FALSE(shouldUpgradeWifi(three(), "Office", {"Office", "Hotspot"}));
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd firmware && pio test -e native -f test_wifi_policy`

Expected: FAIL — `core/wifi_policy.h` not found.

- [ ] **Step 3: Implement**

```cpp
#include "core/wifi_policy.h"

std::vector<size_t> selectWifiCandidates(
    const std::vector<WifiNetwork>& saved,
    const std::vector<std::string>& seen) {
    std::vector<size_t> indices;
    for (size_t i = 0; i < saved.size(); ++i) {
        for (const std::string& ssid : seen) {
            if (ssid == saved[i].ssid) {
                indices.push_back(i);
                break;
            }
        }
    }
    return indices;
}

uint32_t wifiBackoffMs(uint32_t failedCycles) {
    size_t last = sizeof(kWifiBackoffStepsMs) / sizeof(kWifiBackoffStepsMs[0]) - 1;
    size_t i = failedCycles > last ? last : static_cast<size_t>(failedCycles);
    return kWifiBackoffStepsMs[i];
}

bool shouldUpgradeWifi(const std::vector<WifiNetwork>& saved,
                       const std::string& currentSsid,
                       const std::vector<std::string>& seen) {
    if (saved.empty() || currentSsid == saved[0].ssid) {
        return false;
    }
    size_t currentIndex = saved.size();
    for (size_t i = 0; i < saved.size(); ++i) {
        if (saved[i].ssid == currentSsid) {
            currentIndex = i;
            break;
        }
    }
    for (size_t i = 0; i < currentIndex; ++i) {
        for (const std::string& ssid : seen) {
            if (ssid == saved[i].ssid) {
                return true;
            }
        }
    }
    return false;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd firmware && pio test -e native -f test_wifi_policy`

Expected: all 7 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add firmware/include/core/wifi_policy.h \
        firmware/src/core/wifi_policy.cpp \
        firmware/test/test_wifi_policy/test_wifi_policy.cpp
git commit -m "$(cat <<'EOF'
feat(firmware): add WiFi candidate, backoff, and upgrade policy

EOF
)"
```

---

### Task 3: HTML escape

**Files:**
- Create: `firmware/include/core/html_escape.h`
- Create: `firmware/src/core/html_escape.cpp`
- Test: `firmware/test/test_html_escape/test_html_escape.cpp`

**Interfaces:**
- Produces: `std::string escapeHtml(const std::string& raw);`
- Escapes `&` `<` `>` `"` `'` to `&amp;` `&lt;` `&gt;` `&quot;` `&#39;`. Used for every value rendered into the config page.

- [ ] **Step 1: Write the failing test**

```cpp
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
```

- [ ] **Step 2: Run to verify fail** — `pio test -e native -f test_html_escape`

- [ ] **Step 3: Implement** — walk each byte; switch on the five characters; append the rest unchanged (so UTF-8 multi-byte sequences pass through).

- [ ] **Step 4: Run to verify pass**

- [ ] **Step 5: Commit** `feat(firmware): HTML-escape config-page values`

---

### Task 4: NVS store

**Files:**
- Create: `firmware/include/storage/wifi_store.h`
- Create: `firmware/src/storage/wifi_store.cpp`

**Interfaces:**
- Consumes: `serializeWifiNetworks` / `deserializeWifiNetworks`.
- Produces:

```cpp
std::vector<WifiNetwork> loadWifiNetworks();
bool saveWifiNetworks(const std::vector<WifiNetwork>& networks);
bool wifiNetworksKeyExists();
```

Mirror `bus_stop_store.cpp` exactly: namespace `"busaunty"`, key `"wifi"`. Empty save removes the key. `wifiNetworksKeyExists` is the migration guard (true only if the key is present, including after an explicit empty-list save-and-remove? After remove, `isKey` is false — so deleting every network would re-trigger migration. Prevent that: when saving an empty list, write a single sentinel byte the deserializer treats as empty, **or** store a separate `"wifiset"` bool.

Use a companion key `"wifiset"` (bool). `saveWifiNetworks` always sets `wifiset=true`. `wifiNetworksKeyExists` reads `wifiset`. `loadWifiNetworks` still reads `"wifi"` (missing → empty list). That way "user deleted every network" does not resurrect the WiFiManager credentials.

No host test — `Preferences` is ESP32-only. Verify by compiling both board envs after this task's files exist; they will not be linked from `main.cpp` yet, so also add a throwaway include in a later task. For this task, compile is enough if the files are in `src/storage/` (already in both board `build_src_filter`s).

- [ ] **Step 1: Write the two files**, same comment style as `bus_stop_store.cpp`.
- [ ] **Step 2: Build both boards** — `pio run -e sticks3 -e feather_s3_revtft`
- [ ] **Step 3: Commit** `feat(firmware): persist WiFi networks in NVS`

---

### Task 5: Radio state machine

**Files:**
- Create: `firmware/include/net/wifi_link.h`
- Create: `firmware/src/net/wifi_link.cpp`

**Interfaces:**
- Consumes: Task 1–2 types and constants; `WiFi.h`, `esp_wifi.h`.
- Produces:

```cpp
enum class WifiLinkState {
    Scanning,
    FastPath,
    Connecting,
    Connected,
    Backoff,
    ApFallback,
};

void wifiLinkBegin(const std::vector<WifiNetwork>& networks);
void wifiLinkSetNetworks(const std::vector<WifiNetwork>& networks);
void wifiLinkTick(uint32_t nowMs);
void wifiLinkForceScan();          // button press: cancel backoff, scan now
void wifiLinkStartAp(const char* ssid, const char* password);
void wifiLinkStopAp();
void wifiLinkPrepareSleep();       // existing disconnect + WIFI_OFF
void wifiLinkOnWake();             // FastPath if last-good exists, else scan
bool wifiLinkImportStaCredentials(WifiNetwork* out);  // migration read

WifiLinkState wifiLinkState();
bool wifiLinkConnected();
std::string wifiLinkCurrentSsid();
std::string wifiLinkIp();
std::vector<std::string> wifiLinkVisibleSsids();
uint32_t wifiLinkBackoffEndsAt();
```

Behaviour to implement (do not invent extra states):

1. `wifiLinkBegin`: store networks. If empty → `ApFallback` is **not** auto-entered here; `main.cpp` decides to call `wifiLinkStartAp`. Begin with `Scanning` when networks exist, otherwise stay in `Backoff` with `failedCycles` already at cap so we do not hammer an empty list — actually empty list is AP, so `begin` with empty just sits in `Backoff` until `main` starts AP. Simpler: `begin` with empty networks → caller starts AP. `begin` with networks → `Scanning`.
2. `Scanning`: `WiFi.mode(WIFI_STA); WiFi.scanNetworks(/*async=*/true)`. On completion, `selectWifiCandidates`. Empty → increment `failedCycles`, go `Backoff`. Else start `Connecting` at candidates[0].
3. `Connecting`: `WiFi.begin(ssid, password)` (password `.c_str()`, empty string for open). Timeout `kWifiConnectTimeoutMs`. Success → remember last-good, reset `failedCycles`, go `Connected`. Failure → next candidate, or `Backoff`.
4. `Connected`: if `WiFi.status()` drops → `Scanning`. If `now - lastUpgradeCheck >= kWifiUpgradeIntervalMs` and current is not saved[0], run a scan; if `shouldUpgradeWifi` then disconnect and `Connecting` to the better candidate (do not interrupt if a fetch flag is set — expose `wifiLinkSetBusy(bool)` so `main` can hold upgrades during `pollAndRender`).
5. `Backoff`: wait `wifiBackoffMs(failedCycles)`. Expiry → `Scanning`. `wifiLinkForceScan` clears wait and goes `Scanning`.
6. `FastPath`: `WiFi.mode(WIFI_STA); WiFi.begin(lastSsid, lastPass)`. Timeout same as connect. Fail → `Scanning`.
7. `wifiLinkStartAp`: `WiFi.mode(WIFI_AP); WiFi.softAP(ssid, password);` state `ApFallback`.
8. `wifiLinkStopAp`: `WiFi.softAPdisconnect(true);` then `Scanning` if networks exist.
9. `wifiLinkPrepareSleep`: copy the current `enterSleep()` teardown (`disconnect(false, false)` then `mode(WIFI_OFF)`).
10. `wifiLinkOnWake`: `mode(WIFI_STA)` then `FastPath` if last-good is non-empty, else `Scanning`.
11. `wifiLinkImportStaCredentials`: after `WiFi.mode(WIFI_STA)`, `esp_wifi_get_config(WIFI_IF_STA, &cfg)`. If `cfg.sta.ssid[0] != 0`, fill `out` and return true.
12. Visible SSIDs: keep the last completed scan's SSID list for the config page dropdown. A connected rescan uses async scan without changing state if already `Connected` (ESP32 can scan while STA-connected). Add `wifiLinkRequestVisibleScan()` that starts that scan; when it completes, update the list only.

Log the same `[wifi]` serial lines `main.cpp` already uses (associate / disconnect / got ip) — keep `logWifiDiagnostics` in `main.cpp`; the link just `Serial.printf`s state transitions: `[wifi] state scanning|connecting|connected|backoff|ap`.

No host test. After this task the board builds must still succeed even if `main` does not call these yet — the new files compile as part of `src/net/`.

- [ ] **Step 1: Write header + cpp** as specified.
- [ ] **Step 2: Build both boards.**
- [ ] **Step 3: Commit** `feat(firmware): add non-blocking multi-network WiFi link`

---

### Task 6: Config server

**Files:**
- Create: `firmware/include/net/config_server.h`
- Create: `firmware/src/net/config_server.cpp`

**Interfaces:**
- Consumes: `escapeHtml`, credential/stop builders, `wifiLink*` for scan/SSID/IP, `saveWifiNetworks` / `saveBusStops` / `saveAlwaysOn` are **not** called here — the server mutates the in-memory structs and sets dirty flags; `main` persists, same as today's portal.
- Produces:

```cpp
struct ConfigServerData {
    std::vector<WifiNetwork>* networks;
    std::vector<BusStopConfig>* stops;
    bool* alwaysOn;
    bool* networksDirty;
    bool* stopsDirty;
    bool* alwaysOnDirty;
};

void configServerBegin(const ConfigServerData& data);
void configServerTick();
void configServerUnlock(uint32_t nowMs);
bool configServerIsUnlocked(uint32_t nowMs);
uint32_t configServerUnlockRemainingMs(uint32_t nowMs);
void configServerStartCaptiveDns();   // AP mode only
void configServerStopCaptiveDns();
void configServerStartMdns();         // after STA has an IP
void configServerStopMdns();
```

Page behaviour:

- Every route: if `!configServerIsUnlocked(millis())`, respond 200 with a short UTF-8 HTML page: `<p>Press the button on your device to unlock settings.</p>`.
- `GET /` when unlocked: one HTML document, `Content-Type: text/html; charset=utf-8`.
  - Networks table: each row shows escaped SSID, a `type=password` field with **empty value** and placeholder `saved — leave blank to keep` (omit placeholder on a network that has an empty stored password, use `open network — leave blank`), plus POST buttons `up` / `down` / `delete` with a hidden index.
  - Add form: `<select name="scan_ssid">` of `wifiLinkVisibleSsids()` plus an empty "or type SSID" `<input name="ssid">`, password field, submit `add`. A `rescan` button POSTs `/scan`.
  - Stops: four code/name inputs seeded from `*stops`.
  - Always-on checkbox, same copy as today's portal (`kAlwaysOnHtml` in `wifi_portal.cpp` — move the prose here).
- `POST /scan`: `wifiLinkRequestVisibleScan()`, redirect `/`.
- `POST /networks`:
  - `action=add`: append `{ssid: scan_ssid or typed, password}` via `mergeWifiPasswords` + `buildWifiNetworkList`.
  - `action=up`/`down`: swap adjacent indices.
  - `action=delete`: erase index.
  - `action=save_pass`: apply `mergeWifiPasswords` for that index's new password.
  - set `*networksDirty`, call `wifiLinkSetNetworks`.
- `POST /stops`: read `code1..4` / `name1..4`, `buildBusStopList`, set `*stopsDirty`.
- `POST /alwayson`: checkbox present → true, set `*alwaysOnDirty`.
- After every successful POST: `303` redirect to `/`.

Captive DNS: `DNSServer` on port 53, wildcard `"*"`, AP IP (usually `192.168.4.1`). Also handle `GET /generate_204` and `GET /hotspot-detect.html` by redirecting to `/` so iOS/Android pop the page.

mDNS: `MDNS.begin("busaunty")` + `MDNS.addService("http", "tcp", 80)` when STA is up; stop when entering AP or sleep.

- [ ] **Step 1: Write header + cpp.** Keep HTML as raw string fragments; escape every inserted value with `escapeHtml`.
- [ ] **Step 2: Build both boards.**
- [ ] **Step 3: Commit** `feat(firmware): serve a button-unlocked WiFi config page`

---

### Task 7: Display + main loop

**Files:**
- Modify: `firmware/include/ui/display.h`
- Modify: `firmware/src/ui/display.cpp`
- Modify: `firmware/src/main.cpp`
- Delete: `firmware/include/net/wifi_portal.h`, `firmware/src/net/wifi_portal.cpp`
- Modify: `firmware/platformio.ini` — remove the WiFiManager line
- Modify: `README.md` — first-time setup, gestures, hotspot 2.4 GHz note

**Interfaces:**
- Display additions:

```cpp
void displayShowWifiSetup(const std::string& ssid, const std::string& password);
void displayShowConfig(const std::string& url, const std::string& ip,
                       uint32_t remainingMs);
void displayShowWifiOffline();
```

Keep `displayShowWifiSetup` compiling: update the existing one-arg function to two args. Setup screen adds a third line `password`. Config screen is 16px font, three lines: `http://busaunty.local`, the IP, `unlocked Nm` plus `press again for AP`. Offline screen: `No WiFi` / `press to retry`.

`displayShowNoStops` copy becomes: no stops, hold secondary, then open `busaunty.local`.

**`setup()` changes:**

1. Load stops, alwaysOn, networks (`loadWifiNetworks()`).
2. If `!wifiNetworksKeyExists()`: `WiFi.mode(WIFI_STA);` then `wifiLinkImportStaCredentials`. On success, `saveWifiNetworks({imported})` (also sets `wifiset`).
3. `logWifiDiagnostics(); wifiLinkBegin(networks); configServerBegin(...)`.
4. If `networks.empty()`: derive AP password from `WiFi.macAddress()` (parse the 6 octets), `wifiLinkStartAp("BusAuntySetup", password.c_str())`, `configServerUnlock(millis())`, `configServerStartCaptiveDns()`, `displayShowWifiSetup("BusAuntySetup", password)`.
5. Else: `displayShowStatus("Connecting WiFi...");` — do **not** block, do **not** reboot.
6. `noteInteraction();` — NTP waits until first `Connected`.

**`loop()` changes:**

- After `buttonsUpdate` / `powerPoll`, call `wifiLinkTick(millis()); configServerTick();`.
- Any button press: existing wake-from-dim, **and** `wifiLinkForceScan()`.
- Hold Secondary: enter config-screen mode (a `bool inConfigScreen`). Unlock, start mDNS if connected, show `displayShowConfig`. While `inConfigScreen`, Secondary **click** calls `wifiLinkStartAp` + captive DNS + setup screen; Primary click leaves the screen. Auto-leave when unlock expires.
- On `WifiLinkState::Connected` rising edge: `configServerStartMdns(); syncTime();` (once per connection is enough — gate with a `timeSynced` flag reset on disconnect).
- On `Backoff` (and not AP, not config screen): `displayShowWifiOffline()` if not already showing arrivals? Only when `!wifiLinkConnected()` and we would have fetched — do not clobber an arrivals screen that is still useful. Spec says "offline screen". Show it whenever disconnected and not in AP/config. Clear `noStops` / arrivals cache on disconnect so we do not paint stale times without a marker. (Spec's rejected option was keep-stale. So wipe and show offline.)
- Delete the old `WiFi.status() != WL_CONNECTED` reconnect block.
- Delete `openConfigPortal` / `wifiPortalConnect` / `wifiPortalReconfigure`.
- Persist dirty flags from the config server the same way `persistStopsIfChanged` works today.
- `enterSleep`: `configServerStopMdns(); configServerStopCaptiveDns(); wifiLinkPrepareSleep();` then existing display/sleep. On wake: existing display wake, then `wifiLinkOnWake();` — delete the 20s `WiFi.begin()` wait loop.
- Fetch only when `wifiLinkConnected()`.

Parse STA MAC for `deriveApPassword`:

```cpp
uint8_t mac[6];
WiFi.macAddress(mac);
std::string apPass = deriveApPassword(mac);
```

Arduino-ESP32 `WiFi.macAddress(uint8_t*)` fills the STA MAC.

- [ ] **Step 1: Display API + implementations.** Reuse the existing wifi-setup layout; add one line for the password. Config and offline screens follow `displayShowNoStops` (centred 16px lines).
- [ ] **Step 2: Rewrite `setup`/`loop`/`enterSleep` as specified. Delete portal files. Drop WiFiManager from `platformio.ini`.**
- [ ] **Step 3: Update README** — replace First-time setup and the portal-hold rows with the new flow. Add a short "Phone hotspot" subsection: ESP32-S3 is 2.4 GHz only; iPhone Personal Hotspot needs Maximize Compatibility; the hotspot radio sleeps unless the Personal Hotspot screen is open or a client is attached; press a button on the device to scan immediately.
- [ ] **Step 4: Run native tests + both board builds**

```bash
cd firmware
pio test -e native
pio run -e sticks3 -e feather_s3_revtft
```

Expected: all existing + new native tests PASS; both firmware images link with no WiFiManager symbols.

- [ ] **Step 5: Commit** `feat(firmware): replace WiFiManager with a multi-network config page`

---

### Task 8: On-device checklist (manual)

No code unless a bug turns up. Flash `feather_s3_revtft` (the board in use) and walk:

1. **Fresh AP** — erase NVS (`pio run -e feather_s3_revtft -t erase` then upload) or use a wipe path. Device shows `BusAuntySetup` + 8-char password. Phone joins (WPA2), captive page or `192.168.4.1`. Add home WiFi. Device connects and serves `http://busaunty.local` after unlock.
2. **Migration** — flash over a device that still has WiFiManager credentials *without* erasing NVS. It should join home WiFi with no AP.
3. **Priority** — save home (1) and hotspot (2). With both in range, it must join home.
4. **Forced rescan** — leave home, enable hotspot (Maximize Compatibility), press a button; it should join the hotspot within one scan (~2s + connect).
5. **Upgrade** — connected to hotspot, walk into home WiFi; within ~5 minutes it should switch to home.
6. **Offline backoff** — no saved network in range: offline screen, retries at 5s / 15s / 60s; a press retries immediately.
7. **Unlock expiry** — hold secondary, confirm the page works, wait 5 minutes, confirm it locks.
8. **Passwords stay off the wire** — view page source: no saved password appears in HTML.

Tick each item in this task when verified. File bugs as follow-up commits; do not expand scope.

---

## Spec coverage

| Spec requirement | Task |
| --- | --- |
| Credential model, UTF-8, reject separators, merge blank password | 1 |
| Priority candidates, backoff, upgrade decision | 2 |
| HTML escape of rendered values | 3 |
| NVS `wifi` key + migration guard | 4 |
| State machine, fast path, AP, sleep teardown, STA import | 5 |
| LAN page, unlock, mDNS, captive DNS, scan picker, stops, always-on | 6 |
| `main.cpp` rewire, delete portal, drop dependency, README / 5 GHz note | 7 |
| On-device verification | 8 |
| Hidden SSIDs, flash encryption, HTTPS, 802.1X, OTA | out of scope |

## Type consistency

- `WifiNetwork` / `kMaxWifiNetworks` defined in Task 1, used everywhere after.
- Policy constants in Task 2 (`kWifiUnlockWindowMs` = 300000) used by the config server unlock in Task 6.
- `wifiNetworksKeyExists` is the `wifiset` flag, not "is the `wifi` blob non-empty".
- `displayShowWifiSetup` gains a password argument in Task 7; no remaining one-arg callers after the portal is deleted.
