# Multi-Network WiFi (replace WiFiManager) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `tzapu/WiFiManager` with an in-tree multi-network WiFi layer: up to five saved networks tried in a user-set priority order, configured from a web page the device serves on the LAN at `http://busaunty.local`.

**Architecture:** Five new modules replace `src/net/wifi_portal.{h,cpp}`. All decision-making — which network to try, in what order, how long to back off, when to upgrade, how to escape HTML — is pure logic in `src/core/`, compiled and unit-tested on the host by `[env:native]`. The radio, NVS and HTTP server are thin wrappers in `src/net/` and `src/storage/` holding no policy. WiFi stops being a blocking boot step and becomes a state machine that `loop()` advances.

**Tech Stack:** PlatformIO, Arduino-ESP32, Unity for host tests. No new libraries — `WebServer`, `DNSServer`, `ESPmDNS`, `esp_wifi` all ship with the core. `tzapu/WiFiManager` is removed from `lib_deps`.

**Spec:** `docs/superpowers/specs/2026-09-11-wifi-multi-network-design.md`

## Global Constraints

- **No new `lib_deps` entries.** The only `platformio.ini` change is removing `tzapu/WiFiManager@^2.0.17` from `[esp32_base]`.
- **Credentials are rejected, never sanitized.** A password must be stored byte-exact. No trimming, no stripping, no case folding. Contrast `normalizeBusStopName`, which strips — do not copy that pattern into credential code.
- **SSIDs are raw UTF-8.** `Lionel’s iPhone` uses U+2019 and must survive storage and the web form byte-for-byte.
- **Priority is list order.** Index 0 is tried first. There is no separate rank field.
- **Saved passwords are never sent to the browser.** The password field renders with no `value` attribute.
- **`kMaxWifiNetworks = 5`**, SSID 1–32 bytes, password empty (open) or 8–63 chars.
- **Backoff: 5s → 15s → 60s cap.** Reset by any button press.
- **Unlock window: 5 minutes.**
- **Setup AP:** SSID `BusAuntySetup`, WPA2, password derived from the chip MAC.
- **Both boards must build.** `pio run -e sticks3 -e feather_s3_revtft`.
- **Preserve the radio teardown from `enterSleep()`** (`src/main.cpp:216-224`) verbatim. It moves into `linkPrepareForSleep()` in Task 5 — comments and all. Those comments record hard-won light-sleep behaviour; do not rewrite, reorder or "simplify" them.
- Code style: 4-space indent, 80-column-ish, anonymous namespaces for file-local helpers, comments explain *why* not *what*.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `include/core/wifi_credentials.h` / `src/core/wifi_credentials.cpp` | NEW. `WifiNetwork`, validation, list building, blob encoding, edit merging, AP password derivation. Pure. |
| `include/core/wifi_policy.h` / `src/core/wifi_policy.cpp` | NEW. `ScanEntry`, candidate selection by priority, backoff schedule, upgrade decision. Pure. |
| `include/core/html_escape.h` / `src/core/html_escape.cpp` | NEW. Attribute-safe HTML escaping. Pure. |
| `include/storage/wifi_store.h` / `src/storage/wifi_store.cpp` | NEW. NVS load/save + one-shot import of WiFiManager's credentials. |
| `include/net/wifi_link.h` / `src/net/wifi_link.cpp` | NEW. Connection state machine, scan ownership, AP fallback, sleep hooks. |
| `include/net/config_server.h` / `src/net/config_server.cpp` | NEW. `WebServer` + mDNS + `DNSServer`, unlock gate, page rendering and form handling. |
| `include/ui/display.h` / `src/ui/display.cpp` | MODIFY. Add AP-setup-with-password, config-access and offline screens. |
| `src/main.cpp` | MODIFY. Drop blocking connect and the reboot-on-timeout; drive the state machine and server from `loop()`. |
| `include/net/wifi_portal.h` / `src/net/wifi_portal.cpp` | DELETE. |
| `firmware/platformio.ini` | MODIFY. Remove WiFiManager. |
| `README.md` | MODIFY. Provisioning instructions and the 2.4 GHz hotspot gotcha. |

Tasks 1–3 are pure and host-tested. Tasks 4–8 are device code, verified by compiling both boards and by the on-device checklist in Task 9.

---

### Task 1: Credential model and encoding

**Files:**
- Create: `firmware/include/core/wifi_credentials.h`
- Create: `firmware/src/core/wifi_credentials.cpp`
- Test: `firmware/test/test_wifi_credentials/test_wifi_credentials.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `struct WifiNetwork { std::string ssid; std::string password; }`; `kMaxWifiNetworks`; `bool isValidWifiSsid(const std::string&)`; `bool isValidWifiPassword(const std::string&)`; `std::vector<WifiNetwork> buildWifiNetworkList(const std::vector<WifiNetwork>&)`; `std::string serializeWifiNetworks(const std::vector<WifiNetwork>&)`; `std::vector<WifiNetwork> deserializeWifiNetworks(const std::string&)`; `std::vector<WifiNetwork> mergeWifiNetworkEdits(const std::vector<WifiNetwork>& existing, const std::vector<WifiNetwork>& edits)`; `std::string deriveApPassword(const uint8_t mac[6])`.

**Background the implementer needs:** `src/core/bus_stop_config.cpp` already persists a list of records using `\x1f` as a field separator and `\x1e` as a record separator. Reuse that encoding so there is one storage idiom in the tree. But it *strips* offending characters from names; credential code must *reject* instead, because a silently altered password produces a network that fails to authenticate with no visible cause.

- [ ] **Step 1: Write the failing tests**

Create `firmware/test/test_wifi_credentials/test_wifi_credentials.cpp`:

```cpp
#include <unity.h>

#include <string>
#include <vector>

#include "core/wifi_credentials.h"

namespace {

WifiNetwork net(const std::string& ssid, const std::string& password) {
    WifiNetwork n;
    n.ssid = ssid;
    n.password = password;
    return n;
}

}  // namespace

void test_ssid_length_bounds() {
    TEST_ASSERT_FALSE(isValidWifiSsid(""));
    TEST_ASSERT_TRUE(isValidWifiSsid("a"));
    TEST_ASSERT_TRUE(isValidWifiSsid(std::string(32, 'a')));
    TEST_ASSERT_FALSE(isValidWifiSsid(std::string(33, 'a')));
}

void test_ssid_rejects_separator_bytes() {
    // Rejected rather than stripped: a mangled SSID silently never connects.
    TEST_ASSERT_FALSE(isValidWifiSsid(std::string("ho") + '\x1f' + "me"));
    TEST_ASSERT_FALSE(isValidWifiSsid(std::string("ho") + '\x1e' + "me"));
}

void test_password_length_bounds() {
    TEST_ASSERT_TRUE(isValidWifiPassword(""));  // open network
    TEST_ASSERT_FALSE(isValidWifiPassword("short77"));
    TEST_ASSERT_TRUE(isValidWifiPassword("eightchr"));
    TEST_ASSERT_TRUE(isValidWifiPassword(std::string(63, 'x')));
    TEST_ASSERT_FALSE(isValidWifiPassword(std::string(64, 'x')));
}

void test_password_keeps_every_printable_byte() {
    // bus_stop_config strips quotes and angle brackets; passwords must not be.
    const std::string raw = "  <p@ss'\"word>  ";
    std::vector<WifiNetwork> built = buildWifiNetworkList({net("home", raw)});
    TEST_ASSERT_EQUAL_size_t(1, built.size());
    TEST_ASSERT_EQUAL_STRING(raw.c_str(), built[0].password.c_str());
}

void test_build_drops_invalid_rows() {
    std::vector<WifiNetwork> built = buildWifiNetworkList({
        net("", "password"),      // no ssid
        net("home", "short"),     // password too short
        net("office", "officepass"),
    });
    TEST_ASSERT_EQUAL_size_t(1, built.size());
    TEST_ASSERT_EQUAL_STRING("office", built[0].ssid.c_str());
}

void test_build_dedupes_keeping_the_higher_priority_entry() {
    std::vector<WifiNetwork> built = buildWifiNetworkList({
        net("home", "firstpass"),
        net("home", "secondpass"),
    });
    TEST_ASSERT_EQUAL_size_t(1, built.size());
    TEST_ASSERT_EQUAL_STRING("firstpass", built[0].password.c_str());
}

void test_build_truncates_to_the_maximum() {
    std::vector<WifiNetwork> rows;
    for (size_t i = 0; i < kMaxWifiNetworks + 3; ++i) {
        rows.push_back(net("net" + std::to_string(i), "passwordxx"));
    }
    TEST_ASSERT_EQUAL_size_t(kMaxWifiNetworks,
                             buildWifiNetworkList(rows).size());
}

void test_build_preserves_priority_order() {
    std::vector<WifiNetwork> built = buildWifiNetworkList(
        {net("home", "homepass1"), net("office", "officepas"),
         net("hotspot", "hotspotpw")});
    TEST_ASSERT_EQUAL_STRING("home", built[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("office", built[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("hotspot", built[2].ssid.c_str());
}

void test_round_trips_through_the_blob() {
    std::vector<WifiNetwork> before = {net("home", "homepass1"),
                                       net("cafe", ""),
                                       net("office", "officepas")};
    std::vector<WifiNetwork> after =
        deserializeWifiNetworks(serializeWifiNetworks(before));
    TEST_ASSERT_EQUAL_size_t(3, after.size());
    for (size_t i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL_STRING(before[i].ssid.c_str(), after[i].ssid.c_str());
        TEST_ASSERT_EQUAL_STRING(before[i].password.c_str(),
                                 after[i].password.c_str());
    }
}

void test_round_trips_a_utf8_ssid() {
    // U+2019 RIGHT SINGLE QUOTATION MARK, as iOS names a personal hotspot.
    const std::string ssid = "Lionel\xE2\x80\x99s iPhone";
    std::vector<WifiNetwork> after = deserializeWifiNetworks(
        serializeWifiNetworks({net(ssid, "hotspotpw")}));
    TEST_ASSERT_EQUAL_size_t(1, after.size());
    TEST_ASSERT_EQUAL_STRING(ssid.c_str(), after[0].ssid.c_str());
}

void test_empty_blob_yields_no_networks() {
    TEST_ASSERT_EQUAL_size_t(0, deserializeWifiNetworks("").size());
}

void test_corrupt_blob_yields_no_bad_networks() {
    // Truncated mid-record: the ssid survives but the password is gone, so
    // the row is a valid open network; a row with no ssid is dropped.
    TEST_ASSERT_EQUAL_size_t(0, deserializeWifiNetworks("\x1e\x1e").size());
}

void test_blank_password_in_an_edit_keeps_the_stored_one() {
    std::vector<WifiNetwork> existing = {net("home", "homepass1")};
    std::vector<WifiNetwork> merged =
        mergeWifiNetworkEdits(existing, {net("home", "")});
    TEST_ASSERT_EQUAL_size_t(1, merged.size());
    TEST_ASSERT_EQUAL_STRING("homepass1", merged[0].password.c_str());
}

void test_a_supplied_password_replaces_the_stored_one() {
    std::vector<WifiNetwork> existing = {net("home", "homepass1")};
    std::vector<WifiNetwork> merged =
        mergeWifiNetworkEdits(existing, {net("home", "newpass12")});
    TEST_ASSERT_EQUAL_STRING("newpass12", merged[0].password.c_str());
}

void test_a_blank_password_on_an_unknown_ssid_is_an_open_network() {
    std::vector<WifiNetwork> merged =
        mergeWifiNetworkEdits({}, {net("cafe", "")});
    TEST_ASSERT_EQUAL_size_t(1, merged.size());
    TEST_ASSERT_EQUAL_STRING("", merged[0].password.c_str());
}

void test_merge_applies_the_edit_order_as_the_new_priority() {
    std::vector<WifiNetwork> existing = {net("home", "homepass1"),
                                         net("office", "officepas")};
    std::vector<WifiNetwork> merged =
        mergeWifiNetworkEdits(existing, {net("office", ""), net("home", "")});
    TEST_ASSERT_EQUAL_STRING("office", merged[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("officepas", merged[0].password.c_str());
    TEST_ASSERT_EQUAL_STRING("home", merged[1].ssid.c_str());
}

void test_ap_password_is_deterministic_and_wpa2_legal() {
    const uint8_t mac[6] = {0x24, 0x6f, 0x28, 0xab, 0xcd, 0xef};
    std::string first = deriveApPassword(mac);
    TEST_ASSERT_EQUAL_size_t(8, first.size());
    TEST_ASSERT_TRUE(isValidWifiPassword(first));
    TEST_ASSERT_EQUAL_STRING(first.c_str(), deriveApPassword(mac).c_str());
}

void test_ap_password_differs_per_device_and_avoids_lookalikes() {
    const uint8_t a[6] = {0x24, 0x6f, 0x28, 0xab, 0xcd, 0xef};
    const uint8_t b[6] = {0x24, 0x6f, 0x28, 0xab, 0xcd, 0xee};
    TEST_ASSERT_TRUE(deriveApPassword(a) != deriveApPassword(b));
    for (char c : deriveApPassword(a)) {
        TEST_ASSERT_TRUE(c != '0' && c != 'O' && c != '1' && c != 'I' &&
                         c != 'l');
    }
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_ssid_length_bounds);
    RUN_TEST(test_ssid_rejects_separator_bytes);
    RUN_TEST(test_password_length_bounds);
    RUN_TEST(test_password_keeps_every_printable_byte);
    RUN_TEST(test_build_drops_invalid_rows);
    RUN_TEST(test_build_dedupes_keeping_the_higher_priority_entry);
    RUN_TEST(test_build_truncates_to_the_maximum);
    RUN_TEST(test_build_preserves_priority_order);
    RUN_TEST(test_round_trips_through_the_blob);
    RUN_TEST(test_round_trips_a_utf8_ssid);
    RUN_TEST(test_empty_blob_yields_no_networks);
    RUN_TEST(test_corrupt_blob_yields_no_bad_networks);
    RUN_TEST(test_blank_password_in_an_edit_keeps_the_stored_one);
    RUN_TEST(test_a_supplied_password_replaces_the_stored_one);
    RUN_TEST(test_a_blank_password_on_an_unknown_ssid_is_an_open_network);
    RUN_TEST(test_merge_applies_the_edit_order_as_the_new_priority);
    RUN_TEST(test_ap_password_is_deterministic_and_wpa2_legal);
    RUN_TEST(test_ap_password_differs_per_device_and_avoids_lookalikes);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cd firmware && pio test -e native -f test_wifi_credentials`
Expected: FAIL — `core/wifi_credentials.h: No such file or directory`.

- [ ] **Step 3: Write the header**

Create `firmware/include/core/wifi_credentials.h`:

```cpp
// firmware/include/core/wifi_credentials.h
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

constexpr size_t kMaxWifiNetworks = 5;
constexpr size_t kWifiSsidMaxBytes = 32;
constexpr size_t kWifiPasswordMinChars = 8;
constexpr size_t kWifiPasswordMaxChars = 63;
constexpr size_t kApPasswordChars = 8;

// One saved network. Position in the list is its priority: index 0 is tried
// first. An empty password means an open network.
struct WifiNetwork {
    std::string ssid;
    std::string password;
};

// Credentials are validated, never normalized. Trimming whitespace or
// dropping a character from a password yields a network that fails to
// authenticate with nothing on screen to explain why, so anything unusable
// is rejected outright and the user is told.
bool isValidWifiSsid(const std::string& ssid);
bool isValidWifiPassword(const std::string& password);

// Drops invalid rows, de-duplicates by SSID keeping the earliest (highest
// priority) occurrence, and keeps at most kMaxWifiNetworks. Order preserved.
std::vector<WifiNetwork> buildWifiNetworkList(
    const std::vector<WifiNetwork>& rows);

std::string serializeWifiNetworks(const std::vector<WifiNetwork>& networks);
std::vector<WifiNetwork> deserializeWifiNetworks(const std::string& blob);

// Applies form rows over the stored list. A blank password means "keep what
// is stored for this SSID", which is what lets the page render the password
// field empty and still never transmit a secret. The edit order becomes the
// new priority order.
std::vector<WifiNetwork> mergeWifiNetworkEdits(
    const std::vector<WifiNetwork>& existing,
    const std::vector<WifiNetwork>& edits);

// Per-device setup-AP password, so the AP can be WPA2 without a shared secret
// baked into the firmware. Deterministic, so the screen and the radio agree
// without storing it.
std::string deriveApPassword(const uint8_t mac[6]);
```

- [ ] **Step 4: Write the implementation**

Create `firmware/src/core/wifi_credentials.cpp`:

```cpp
#include "core/wifi_credentials.h"

namespace {

// Same encoding as bus_stop_config.cpp, so the tree has one storage idiom.
constexpr char kFieldSep = '\x1f';
constexpr char kRecordSep = '\x1e';

// Omits 0/O/1/I/l: the password is read off a small screen and typed into a
// phone, where a lookalike costs a failed join with no diagnostic.
constexpr char kApAlphabet[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
constexpr size_t kApAlphabetBits = 5;  // 32 symbols

bool hasSeparator(const std::string& value) {
    for (char c : value) {
        if (c == kFieldSep || c == kRecordSep) {
            return true;
        }
    }
    return false;
}

}  // namespace

bool isValidWifiSsid(const std::string& ssid) {
    if (ssid.empty() || ssid.size() > kWifiSsidMaxBytes) {
        return false;
    }
    return !hasSeparator(ssid);
}

bool isValidWifiPassword(const std::string& password) {
    if (password.empty()) {
        return true;  // open network
    }
    if (password.size() < kWifiPasswordMinChars ||
        password.size() > kWifiPasswordMaxChars) {
        return false;
    }
    return !hasSeparator(password);
}

std::vector<WifiNetwork> buildWifiNetworkList(
    const std::vector<WifiNetwork>& rows) {
    std::vector<WifiNetwork> networks;
    for (const WifiNetwork& row : rows) {
        if (networks.size() >= kMaxWifiNetworks) {
            break;
        }
        if (!isValidWifiSsid(row.ssid) || !isValidWifiPassword(row.password)) {
            continue;
        }
        bool duplicate = false;
        for (const WifiNetwork& kept : networks) {
            if (kept.ssid == row.ssid) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        networks.push_back(row);
    }
    return networks;
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
    size_t start = 0;
    while (start <= blob.size()) {
        size_t recordEnd = blob.find(kRecordSep, start);
        if (recordEnd == std::string::npos) {
            recordEnd = blob.size();
        }
        std::string record = blob.substr(start, recordEnd - start);
        size_t fieldEnd = record.find(kFieldSep);
        WifiNetwork row;
        if (fieldEnd == std::string::npos) {
            row.ssid = record;
        } else {
            row.ssid = record.substr(0, fieldEnd);
            row.password = record.substr(fieldEnd + 1);
        }
        rows.push_back(row);
        if (recordEnd == blob.size()) {
            break;
        }
        start = recordEnd + 1;
    }
    // Re-validate, so a corrupt or truncated blob cannot yield bad entries.
    return buildWifiNetworkList(rows);
}

std::vector<WifiNetwork> mergeWifiNetworkEdits(
    const std::vector<WifiNetwork>& existing,
    const std::vector<WifiNetwork>& edits) {
    std::vector<WifiNetwork> rows;
    rows.reserve(edits.size());
    for (const WifiNetwork& edit : edits) {
        WifiNetwork row = edit;
        if (row.password.empty()) {
            for (const WifiNetwork& stored : existing) {
                if (stored.ssid == row.ssid) {
                    row.password = stored.password;
                    break;
                }
            }
        }
        rows.push_back(row);
    }
    return buildWifiNetworkList(rows);
}

std::string deriveApPassword(const uint8_t mac[6]) {
    // The OUI (first three bytes) is shared across every device from the same
    // vendor, so only the device-specific tail feeds the password. Five bytes
    // is exactly kApPasswordChars * kApAlphabetBits bits.
    uint64_t bits = 0;
    for (size_t i = 1; i < 6; ++i) {
        bits = (bits << 8) | mac[i];
    }
    std::string password(kApPasswordChars, kApAlphabet[0]);
    for (size_t i = 0; i < kApPasswordChars; ++i) {
        password[kApPasswordChars - 1 - i] = kApAlphabet[bits & 0x1f];
        bits >>= kApAlphabetBits;
    }
    return password;
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cd firmware && pio test -e native -f test_wifi_credentials`
Expected: PASS, 18 tests.

- [ ] **Step 6: Commit**

```bash
git add firmware/include/core/wifi_credentials.h firmware/src/core/wifi_credentials.cpp firmware/test/test_wifi_credentials
git commit -m "feat(firmware): add a validated, order-is-priority WiFi credential model"
```

---

### Task 2: Selection and backoff policy

**Files:**
- Create: `firmware/include/core/wifi_policy.h`
- Create: `firmware/src/core/wifi_policy.cpp`
- Test: `firmware/test/test_wifi_policy/test_wifi_policy.cpp`

**Interfaces:**
- Consumes: `WifiNetwork`, `kMaxWifiNetworks` from Task 1.
- Produces: `struct ScanEntry { std::string ssid; int32_t rssi; }`; `std::vector<size_t> selectCandidates(const std::vector<WifiNetwork>& saved, const std::vector<ScanEntry>& scan)`; `uint32_t backoffDelayMs(size_t consecutiveFailures)`; `bool shouldUpgrade(const std::vector<WifiNetwork>& saved, const std::vector<ScanEntry>& scan, const std::string& connectedSsid)`; `bool dueForUpgradeCheck(uint32_t nowMs, uint32_t lastCheckMs)`; `kUpgradeCheckIntervalMs`.

**Why this is a separate module from the radio:** standing in a kitchen toggling a hotspot is a miserable test loop. Every ordering and timing rule lives here so it can be checked on the host in milliseconds; `wifi_link.cpp` in Task 5 only executes the answers.

- [ ] **Step 1: Write the failing tests**

Create `firmware/test/test_wifi_policy/test_wifi_policy.cpp`:

```cpp
#include <unity.h>

#include <string>
#include <vector>

#include "core/wifi_credentials.h"
#include "core/wifi_policy.h"

namespace {

WifiNetwork net(const std::string& ssid) {
    WifiNetwork n;
    n.ssid = ssid;
    n.password = "passwordx";
    return n;
}

ScanEntry seen(const std::string& ssid, int32_t rssi) {
    ScanEntry e;
    e.ssid = ssid;
    e.rssi = rssi;
    return e;
}

// home is top priority, then office, then the hotspot.
std::vector<WifiNetwork> saved() {
    return {net("home"), net("office"), net("hotspot")};
}

}  // namespace

void test_priority_beats_signal_strength() {
    // The hotspot is far louder, but home is the user's first choice.
    std::vector<size_t> candidates =
        selectCandidates(saved(), {seen("hotspot", -30), seen("home", -80)});
    TEST_ASSERT_EQUAL_size_t(2, candidates.size());
    TEST_ASSERT_EQUAL_size_t(0, candidates[0]);  // home
    TEST_ASSERT_EQUAL_size_t(2, candidates[1]);  // hotspot
}

void test_saved_but_not_visible_is_excluded() {
    std::vector<size_t> candidates =
        selectCandidates(saved(), {seen("office", -60)});
    TEST_ASSERT_EQUAL_size_t(1, candidates.size());
    TEST_ASSERT_EQUAL_size_t(1, candidates[0]);
}

void test_visible_but_not_saved_is_ignored() {
    std::vector<size_t> candidates =
        selectCandidates(saved(), {seen("StarbucksWiFi", -40)});
    TEST_ASSERT_EQUAL_size_t(0, candidates.size());
}

void test_nothing_saved_yields_nothing() {
    TEST_ASSERT_EQUAL_size_t(0, selectCandidates({}, {seen("home", -40)}).size());
}

void test_empty_scan_yields_nothing() {
    TEST_ASSERT_EQUAL_size_t(0, selectCandidates(saved(), {}).size());
}

void test_duplicate_scan_entries_do_not_duplicate_candidates() {
    // Mesh networks and repeaters put the same SSID on several channels.
    std::vector<size_t> candidates =
        selectCandidates(saved(), {seen("home", -70), seen("home", -45)});
    TEST_ASSERT_EQUAL_size_t(1, candidates.size());
    TEST_ASSERT_EQUAL_size_t(0, candidates[0]);
}

void test_ssid_matching_is_case_sensitive() {
    // 802.11 SSIDs are opaque byte strings; "Home" is a different network.
    TEST_ASSERT_EQUAL_size_t(0, selectCandidates(saved(), {seen("HOME", -40)}).size());
}

void test_backoff_climbs_then_caps() {
    TEST_ASSERT_EQUAL_UINT32(5000, backoffDelayMs(0));
    TEST_ASSERT_EQUAL_UINT32(15000, backoffDelayMs(1));
    TEST_ASSERT_EQUAL_UINT32(60000, backoffDelayMs(2));
    TEST_ASSERT_EQUAL_UINT32(60000, backoffDelayMs(3));
    TEST_ASSERT_EQUAL_UINT32(60000, backoffDelayMs(99));
}

void test_upgrade_fires_when_a_higher_priority_network_appears() {
    TEST_ASSERT_TRUE(shouldUpgrade(
        saved(), {seen("hotspot", -30), seen("office", -70)}, "hotspot"));
}

void test_upgrade_does_not_fire_on_the_top_priority_network() {
    TEST_ASSERT_FALSE(shouldUpgrade(
        saved(), {seen("home", -70), seen("office", -30)}, "home"));
}

void test_upgrade_does_not_fire_for_a_lower_priority_network() {
    TEST_ASSERT_FALSE(shouldUpgrade(
        saved(), {seen("office", -70), seen("hotspot", -30)}, "office"));
}

void test_upgrade_does_not_fire_for_an_unsaved_connection() {
    // Connected to something we have no opinion about; leave it alone.
    TEST_ASSERT_FALSE(
        shouldUpgrade(saved(), {seen("home", -40)}, "SomeOtherNet"));
}

void test_upgrade_check_interval_and_rollover() {
    TEST_ASSERT_FALSE(dueForUpgradeCheck(1000, 0));
    TEST_ASSERT_TRUE(dueForUpgradeCheck(kUpgradeCheckIntervalMs, 0));
    // Unsigned subtraction makes the millis() wrap a non-event.
    uint32_t last = 0xFFFFFFFFu - 1000u;
    TEST_ASSERT_TRUE(dueForUpgradeCheck(last + kUpgradeCheckIntervalMs, last));
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_priority_beats_signal_strength);
    RUN_TEST(test_saved_but_not_visible_is_excluded);
    RUN_TEST(test_visible_but_not_saved_is_ignored);
    RUN_TEST(test_nothing_saved_yields_nothing);
    RUN_TEST(test_empty_scan_yields_nothing);
    RUN_TEST(test_duplicate_scan_entries_do_not_duplicate_candidates);
    RUN_TEST(test_ssid_matching_is_case_sensitive);
    RUN_TEST(test_backoff_climbs_then_caps);
    RUN_TEST(test_upgrade_fires_when_a_higher_priority_network_appears);
    RUN_TEST(test_upgrade_does_not_fire_on_the_top_priority_network);
    RUN_TEST(test_upgrade_does_not_fire_for_a_lower_priority_network);
    RUN_TEST(test_upgrade_does_not_fire_for_an_unsaved_connection);
    RUN_TEST(test_upgrade_check_interval_and_rollover);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cd firmware && pio test -e native -f test_wifi_policy`
Expected: FAIL — `core/wifi_policy.h: No such file or directory`.

- [ ] **Step 3: Write the header**

Create `firmware/include/core/wifi_policy.h`:

```cpp
// firmware/include/core/wifi_policy.h
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/wifi_credentials.h"

// One entry from a radio scan, reduced to what any decision here needs.
struct ScanEntry {
    std::string ssid;
    int32_t rssi = 0;
};

// Retry schedule after a full cycle finds nothing. Climbs so a device left
// out of range all day is not scanning every five seconds, and caps so it
// still recovers on its own; a button press resets the count to zero.
constexpr uint32_t kBackoffStepsMs[] = {5000, 15000, 60000};

// How often to look for a better network while connected to a lesser one.
// A scan briefly drops the station connection, so this is deliberately far
// longer than the 30s arrivals poll.
constexpr uint32_t kUpgradeCheckIntervalMs = 300000;

// Indices into `saved`, in the order they should be tried: saved order --
// the user's priority -- restricted to SSIDs the scan actually saw.
//
// Priority rather than signal strength is the whole point. Sorting by RSSI
// makes a phone hotspot beat the house router in the same room, quietly
// spending cellular data.
std::vector<size_t> selectCandidates(const std::vector<WifiNetwork>& saved,
                                     const std::vector<ScanEntry>& scan);

uint32_t backoffDelayMs(size_t consecutiveFailures);

// True when the current connection is a saved network that something visible
// outranks. False when already on the best available, or when connected to a
// network that is not saved at all.
bool shouldUpgrade(const std::vector<WifiNetwork>& saved,
                   const std::vector<ScanEntry>& scan,
                   const std::string& connectedSsid);

bool dueForUpgradeCheck(uint32_t nowMs, uint32_t lastCheckMs);
```

- [ ] **Step 4: Write the implementation**

Create `firmware/src/core/wifi_policy.cpp`:

```cpp
#include "core/wifi_policy.h"

namespace {

constexpr size_t kBackoffStepCount =
    sizeof(kBackoffStepsMs) / sizeof(kBackoffStepsMs[0]);

bool scanContains(const std::vector<ScanEntry>& scan, const std::string& ssid) {
    for (const ScanEntry& entry : scan) {
        // Byte comparison: 802.11 SSIDs are opaque octet strings, so "Home"
        // and "home" really are different networks.
        if (entry.ssid == ssid) {
            return true;
        }
    }
    return false;
}

}  // namespace

std::vector<size_t> selectCandidates(const std::vector<WifiNetwork>& saved,
                                     const std::vector<ScanEntry>& scan) {
    std::vector<size_t> candidates;
    // Walking `saved` rather than `scan` is what makes the result
    // priority-ordered, and it also makes repeated scan entries for one SSID
    // (mesh nodes, repeaters) collapse to a single candidate for free.
    for (size_t i = 0; i < saved.size(); ++i) {
        if (scanContains(scan, saved[i].ssid)) {
            candidates.push_back(i);
        }
    }
    return candidates;
}

uint32_t backoffDelayMs(size_t consecutiveFailures) {
    size_t step = consecutiveFailures < kBackoffStepCount
                      ? consecutiveFailures
                      : kBackoffStepCount - 1;
    return kBackoffStepsMs[step];
}

bool shouldUpgrade(const std::vector<WifiNetwork>& saved,
                   const std::vector<ScanEntry>& scan,
                   const std::string& connectedSsid) {
    std::vector<size_t> candidates = selectCandidates(saved, scan);
    if (candidates.empty()) {
        return false;
    }
    for (size_t i = 0; i < saved.size(); ++i) {
        if (saved[i].ssid == connectedSsid) {
            return candidates[0] < i;
        }
    }
    // Connected to something not in the list -- the user never expressed a
    // preference, so do not second-guess a working connection.
    return false;
}

bool dueForUpgradeCheck(uint32_t nowMs, uint32_t lastCheckMs) {
    return nowMs - lastCheckMs >= kUpgradeCheckIntervalMs;
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cd firmware && pio test -e native -f test_wifi_policy`
Expected: PASS, 13 tests.

- [ ] **Step 6: Run the whole host suite to check nothing regressed**

Run: `cd firmware && pio test -e native`
Expected: all suites pass.

- [ ] **Step 7: Commit**

```bash
git add firmware/include/core/wifi_policy.h firmware/src/core/wifi_policy.cpp firmware/test/test_wifi_policy
git commit -m "feat(firmware): pick WiFi networks by user priority, with capped backoff"
```

---

### Task 3: Attribute-safe HTML escaping

**Files:**
- Create: `firmware/include/core/html_escape.h`
- Create: `firmware/src/core/html_escape.cpp`
- Test: `firmware/test/test_html_escape/test_html_escape.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `std::string htmlEscape(const std::string& raw)`.

**Why this is its own module:** `normalizeBusStopName` strips `'`, `<` and `>` today precisely because the WiFiManager portal rendered values into `value='...'` unescaped. SSIDs cannot be stripped — `Bob's WiFi` is a real network name — so the new server must escape properly instead. Escaping bugs are silent and security-relevant, which makes this worth a test even though it is twelve lines.

- [ ] **Step 1: Write the failing tests**

Create `firmware/test/test_html_escape/test_html_escape.cpp`:

```cpp
#include <unity.h>

#include <string>

#include "core/html_escape.h"

void test_passes_ordinary_text_through() {
    TEST_ASSERT_EQUAL_STRING("Home WiFi 5", htmlEscape("Home WiFi 5").c_str());
}

void test_escapes_every_attribute_breaking_character() {
    TEST_ASSERT_EQUAL_STRING("&amp;", htmlEscape("&").c_str());
    TEST_ASSERT_EQUAL_STRING("&lt;", htmlEscape("<").c_str());
    TEST_ASSERT_EQUAL_STRING("&gt;", htmlEscape(">").c_str());
    TEST_ASSERT_EQUAL_STRING("&quot;", htmlEscape("\"").c_str());
    TEST_ASSERT_EQUAL_STRING("&#39;", htmlEscape("'").c_str());
}

void test_escapes_the_ampersand_first() {
    // Escaping '<' before '&' would yield "&amp;lt;" and render as "&lt;".
    TEST_ASSERT_EQUAL_STRING("&amp;lt;", htmlEscape("&lt;").c_str());
}

void test_leaves_utf8_bytes_alone() {
    // The curly apostrophe in an iPhone hotspot name is multi-byte UTF-8 and
    // needs no escaping; mangling it makes the network unjoinable.
    const std::string ssid = "Lionel\xE2\x80\x99s iPhone";
    TEST_ASSERT_EQUAL_STRING(ssid.c_str(), htmlEscape(ssid).c_str());
}

void test_escapes_an_apostrophe_in_a_real_ssid() {
    TEST_ASSERT_EQUAL_STRING("Bob&#39;s WiFi", htmlEscape("Bob's WiFi").c_str());
}

void test_empty_stays_empty() {
    TEST_ASSERT_EQUAL_STRING("", htmlEscape("").c_str());
}

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_passes_ordinary_text_through);
    RUN_TEST(test_escapes_every_attribute_breaking_character);
    RUN_TEST(test_escapes_the_ampersand_first);
    RUN_TEST(test_leaves_utf8_bytes_alone);
    RUN_TEST(test_escapes_an_apostrophe_in_a_real_ssid);
    RUN_TEST(test_empty_stays_empty);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cd firmware && pio test -e native -f test_html_escape`
Expected: FAIL — `core/html_escape.h: No such file or directory`.

- [ ] **Step 3: Write the header and implementation**

Create `firmware/include/core/html_escape.h`:

```cpp
// firmware/include/core/html_escape.h
#pragma once
#include <string>

// Escapes the five characters that can break out of an HTML attribute or a
// text node. Needed because SSIDs cannot be sanitized on the way in -- an
// apostrophe is legal in a network name, and altering it would make the
// network unjoinable -- so they must be made safe on the way out instead.
//
// UTF-8 sequences pass through untouched; the page is served as utf-8.
std::string htmlEscape(const std::string& raw);
```

Create `firmware/src/core/html_escape.cpp`:

```cpp
#include "core/html_escape.h"

std::string htmlEscape(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        switch (c) {
            // '&' must come first in intent as well as in this switch: a
            // two-pass escape that handled '<' before '&' would double-encode.
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cd firmware && pio test -e native -f test_html_escape`
Expected: PASS, 6 tests.

- [ ] **Step 5: Commit**

```bash
git add firmware/include/core/html_escape.h firmware/src/core/html_escape.cpp firmware/test/test_html_escape
git commit -m "feat(firmware): escape HTML output so SSIDs need no sanitizing"
```

---

### Task 4: Credential storage and the WiFiManager import

**Files:**
- Create: `firmware/include/storage/wifi_store.h`
- Create: `firmware/src/storage/wifi_store.cpp`

**Interfaces:**
- Consumes: `WifiNetwork`, `serializeWifiNetworks`, `deserializeWifiNetworks`, `isValidWifiSsid` from Task 1.
- Produces: `std::vector<WifiNetwork> loadWifiNetworks()`; `bool saveWifiNetworks(const std::vector<WifiNetwork>&)`; `bool hasStoredWifiNetworks()`; `bool importLegacyWifiNetwork(WifiNetwork* out)`.

**Background:** `src/storage/bus_stop_store.cpp` is the model to copy — same `busaunty` Preferences namespace, same "remove the key rather than store an empty value" behaviour so "nothing configured" stays one distinguishable state. There are no host tests here; `storage/` is excluded from `[env:native]` because `Preferences.h` is ESP32-only. All the logic worth testing already lives in Task 1.

**On the import:** the ESP-IDF WiFi driver loads the station config saved by a previous `WiFi.begin()` out of its own NVS partition at `esp_wifi_start()`, which is what WiFiManager left behind. Reading it back with `esp_wifi_get_config()` after `WiFi.mode(WIFI_STA)` should therefore hand us the home network. **This is unverified — see Step 4.**

- [ ] **Step 1: Write the header**

Create `firmware/include/storage/wifi_store.h`:

```cpp
// firmware/include/storage/wifi_store.h
#pragma once
#include <vector>

#include "core/wifi_credentials.h"

std::vector<WifiNetwork> loadWifiNetworks();
bool saveWifiNetworks(const std::vector<WifiNetwork>& networks);

// Whether this device has ever been provisioned under the new scheme. Drives
// the one-shot import below, and distinguishes "no networks yet" from "the
// user deleted them all".
bool hasStoredWifiNetworks();

// Reads the credentials WiFiManager left in the ESP-IDF driver's own NVS, so
// a device flashed with this firmware keeps working on the network it was
// already using. Returns false when there is nothing usable to import.
//
// Call only after WiFi.mode(WIFI_STA); the driver populates its config from
// NVS when the station interface starts.
bool importLegacyWifiNetwork(WifiNetwork* out);
```

- [ ] **Step 2: Write the implementation**

Create `firmware/src/storage/wifi_store.cpp`:

```cpp
// firmware/src/storage/wifi_store.cpp
#include "storage/wifi_store.h"

#include <Preferences.h>
#include <esp_wifi.h>

#include <cstring>

namespace {

constexpr char kNamespace[] = "busaunty";
constexpr char kNetworksKey[] = "wifi";

// esp_wifi's ssid/password fields are fixed-size and are not required to be
// NUL-terminated when full, so the length has to be bounded explicitly.
std::string fromFixedField(const uint8_t* field, size_t capacity) {
    size_t length = 0;
    while (length < capacity && field[length] != '\0') {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(field), length);
}

}  // namespace

std::vector<WifiNetwork> loadWifiNetworks() {
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
        return {};
    }
    String blob = prefs.getString(kNetworksKey, "");
    prefs.end();
    return deserializeWifiNetworks(std::string(blob.c_str()));
}

bool saveWifiNetworks(const std::vector<WifiNetwork>& networks) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
        return false;
    }
    std::string blob = serializeWifiNetworks(networks);
    bool ok;
    if (blob.empty()) {
        // Same convention as bus_stop_store: clearing every network drops the
        // key rather than storing an empty value, so hasStoredWifiNetworks()
        // stays meaningful.
        ok = prefs.remove(kNetworksKey) || !prefs.isKey(kNetworksKey);
    } else {
        ok = prefs.putString(kNetworksKey, blob.c_str()) == blob.size();
    }
    prefs.end();
    return ok;
}

bool hasStoredWifiNetworks() {
    Preferences prefs;
    if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
        return false;
    }
    bool present = prefs.isKey(kNetworksKey);
    prefs.end();
    return present;
}

bool importLegacyWifiNetwork(WifiNetwork* out) {
    wifi_config_t config;
    std::memset(&config, 0, sizeof(config));
    if (esp_wifi_get_config(WIFI_IF_STA, &config) != ESP_OK) {
        return false;
    }
    WifiNetwork imported;
    imported.ssid = fromFixedField(config.sta.ssid, sizeof(config.sta.ssid));
    imported.password =
        fromFixedField(config.sta.password, sizeof(config.sta.password));
    if (!isValidWifiSsid(imported.ssid) ||
        !isValidWifiPassword(imported.password)) {
        return false;
    }
    if (out != nullptr) {
        *out = imported;
    }
    return true;
}
```

- [ ] **Step 3: Verify both boards still compile**

Run: `cd firmware && pio run -e sticks3 -e feather_s3_revtft`
Expected: SUCCESS. Nothing calls the new module yet; this only proves the includes and types are right.

- [ ] **Step 4: Verify the import on a real device (cannot be tested on the host)**

On a device currently running the old firmware with a working saved network, flash a scratch sketch — or add a temporary `Serial.printf` in `setup()` after `WiFi.mode(WIFI_STA)` — that calls `importLegacyWifiNetwork()` and prints the SSID it found.

Expected: the SSID of the network the device is already using.

If it prints nothing, the driver is not populating its config from NVS at start. Record that in the plan, drop `importLegacyWifiNetwork` from Task 8's boot path, and accept one-time re-provisioning through the AP fallback. Do not spend more than one debugging session on this — it saves the user one setup, nothing more.

- [ ] **Step 5: Commit**

```bash
git add firmware/include/storage/wifi_store.h firmware/src/storage/wifi_store.cpp
git commit -m "feat(firmware): persist WiFi networks and import WiFiManager's"
```

---

### Task 5: Connection state machine

**Files:**
- Create: `firmware/include/net/wifi_link.h`
- Create: `firmware/src/net/wifi_link.cpp`

**Interfaces:**
- Consumes: Task 1 (`WifiNetwork`, `deriveApPassword`), Task 2 (`ScanEntry`, `selectCandidates`, `backoffDelayMs`, `shouldUpgrade`, `dueForUpgradeCheck`).
- Produces: `enum class LinkState { FastPath, Scanning, Connecting, Connected, Backoff, ApFallback }`; `struct LinkStatus { LinkState state; std::string ssid; std::string ip; }`; `void linkBegin(const std::vector<WifiNetwork>&)`; `void linkSetNetworks(const std::vector<WifiNetwork>&)`; `void linkUpdate()`; `LinkStatus linkStatus()`; `bool linkConnected()`; `void linkForceRescan()`; `void linkStartAp()`; `void linkStopAp()`; `void linkPrepareForSleep()`; `void linkResumeFromSleep()`; `const std::string& linkApSsid()`; `const std::string& linkApPassword()`; `void linkRequestScan()`; `std::vector<ScanEntry> linkScanResults()`.

**The one rule that matters:** `linkUpdate()` must never block. It is called every pass of `loop()` alongside button handling and rendering; anything that waits in a `while` loop freezes the UI. Every wait is expressed as "has enough time passed since the timestamp I stored", never as `delay()`.

- [ ] **Step 1: Write the header**

Create `firmware/include/net/wifi_link.h`:

```cpp
// firmware/include/net/wifi_link.h
#pragma once
#include <string>
#include <vector>

#include "core/wifi_credentials.h"
#include "core/wifi_policy.h"

// Where the connection currently is. Exposed so the UI can distinguish "still
// trying" from "given up for now", which the old binary connected/not-connected
// screen could not.
enum class LinkState {
    FastPath,    // blind retry of the last network that worked, no scan
    Scanning,
    Connecting,
    Connected,
    Backoff,     // nothing in range; waiting before the next cycle
    ApFallback,  // serving the setup AP
};

struct LinkStatus {
    LinkState state = LinkState::Scanning;
    std::string ssid;  // set when Connected
    std::string ip;    // set when Connected
};

void linkBegin(const std::vector<WifiNetwork>& networks);
// Replaces the list after the config page saved, and restarts the search so
// a newly added network is tried immediately.
void linkSetNetworks(const std::vector<WifiNetwork>& networks);

// Advances the state machine. Call once per pass of loop(). Never blocks.
void linkUpdate();

LinkStatus linkStatus();
bool linkConnected();

// Clears the backoff and scans now. This is the whole mechanism for catching
// a phone hotspot that was switched on a moment ago: iOS only keeps the
// hotspot radio up while its settings screen is open or a client is attached,
// so waiting out a 60s backoff can miss the window entirely.
void linkForceRescan();

void linkStartAp();
void linkStopAp();

// Drops the radio for light sleep, and brings it back afterwards. The
// teardown order here is load-bearing -- see the comments in the body.
void linkPrepareForSleep();
void linkResumeFromSleep();

const std::string& linkApSsid();
const std::string& linkApPassword();

// One-shot scan for the config page's SSID picker, separate from the state
// machine's own scanning. Asynchronous: the results appear a second or two
// after the request, which is why the page reloads itself rather than
// rendering the picker straight away.
void linkRequestScan();
std::vector<ScanEntry> linkScanResults();
```

- [ ] **Step 2: Write the implementation**

Create `firmware/src/net/wifi_link.cpp`:

```cpp
// firmware/src/net/wifi_link.cpp
#include "net/wifi_link.h"

#include <Arduino.h>
#include <WiFi.h>

namespace {

constexpr char kApSsid[] = "BusAuntySetup";
// A scan takes ~2s and a failed associate can sit for a while, so this is
// generous; the state machine is non-blocking, so a long timeout costs
// responsiveness nothing.
constexpr uint32_t kConnectTimeoutMs = 10000;
// The fast path is a bet that nothing moved since the last connection. Keep
// it short -- when the bet is wrong, this delay is pure waste before the scan.
constexpr uint32_t kFastPathTimeoutMs = 4000;

std::vector<WifiNetwork> networks;
LinkState state = LinkState::Scanning;
std::string apPassword;

// Survives light sleep, because light sleep retains RAM. Lost on reboot,
// which is correct: a cold boot has no reason to believe anything.
std::string lastGoodSsid;

std::vector<size_t> candidates;
size_t candidateIndex = 0;
size_t consecutiveFailures = 0;
uint32_t stateEnteredMs = 0;
uint32_t backoffUntilMs = 0;
uint32_t lastUpgradeCheckMs = 0;
bool upgradeScanInFlight = false;

bool pageScanRequested = false;
bool pageScanInFlight = false;
std::vector<ScanEntry> pageScanResults;

void enter(LinkState next) {
    state = next;
    stateEnteredMs = millis();
}

bool elapsed(uint32_t sinceMs, uint32_t timeoutMs) {
    return millis() - sinceMs >= timeoutMs;
}

void beginConnect(const WifiNetwork& network) {
    WiFi.mode(WIFI_STA);
    // An open network needs a null key, not an empty string.
    WiFi.begin(network.ssid.c_str(),
               network.password.empty() ? nullptr : network.password.c_str());
    Serial.printf("[wifi] connecting to %s\n", network.ssid.c_str());
}

void startScan() {
    // A scan needs a station interface. During AP fallback the AP must stay
    // up while it happens, or the phone filling in the form gets dropped
    // mid-form -- hence AP_STA rather than STA here.
    WiFi.mode(state == LinkState::ApFallback ? WIFI_AP_STA : WIFI_STA);
    WiFi.scanDelete();
    WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false);
}

std::vector<ScanEntry> collectScan(int16_t count) {
    std::vector<ScanEntry> entries;
    entries.reserve(count > 0 ? count : 0);
    for (int16_t i = 0; i < count; ++i) {
        ScanEntry entry;
        entry.ssid = std::string(WiFi.SSID(i).c_str());
        entry.rssi = WiFi.RSSI(i);
        if (!entry.ssid.empty()) {
            entries.push_back(entry);
        }
    }
    return entries;
}

// Runs the config page's SSID scan outside the state machine's own scanning.
// It has to work both while connected (adding a network from the sofa) and
// during AP fallback (a fresh device, where the picker matters most), neither
// of which passes through the Scanning state.
void servicePageScan() {
    if (!pageScanRequested) {
        return;
    }
    if (!pageScanInFlight) {
        startScan();
        pageScanInFlight = true;
        return;
    }
    int16_t count = WiFi.scanComplete();
    if (count == WIFI_SCAN_RUNNING) {
        return;
    }
    pageScanResults = count > 0 ? collectScan(count) : std::vector<ScanEntry>();
    WiFi.scanDelete();
    pageScanRequested = false;
    pageScanInFlight = false;
}

void enterBackoff() {
    uint32_t delayMs = backoffDelayMs(consecutiveFailures);
    ++consecutiveFailures;
    backoffUntilMs = millis() + delayMs;
    Serial.printf("[wifi] nothing in range, waiting %lums\n",
                  static_cast<unsigned long>(delayMs));
    enter(LinkState::Backoff);
}

void tryNextCandidate() {
    if (candidateIndex >= candidates.size()) {
        enterBackoff();
        return;
    }
    beginConnect(networks[candidates[candidateIndex]]);
    enter(LinkState::Connecting);
}

void onConnected() {
    consecutiveFailures = 0;
    lastGoodSsid = std::string(WiFi.SSID().c_str());
    lastUpgradeCheckMs = millis();
    Serial.printf("[wifi] connected to %s as %s\n", lastGoodSsid.c_str(),
                  WiFi.localIP().toString().c_str());
    enter(LinkState::Connected);
}

void restartSearch() {
    candidates.clear();
    candidateIndex = 0;
    consecutiveFailures = 0;
    upgradeScanInFlight = false;
    if (networks.empty()) {
        // Nothing to connect to and no LAN to be reached on: the AP is the
        // only way in. This is the one place it comes up unasked.
        linkStartAp();
        return;
    }
    startScan();
    enter(LinkState::Scanning);
}

void handleScanComplete(int16_t count) {
    std::vector<ScanEntry> entries = collectScan(count);
    WiFi.scanDelete();

    if (upgradeScanInFlight) {
        upgradeScanInFlight = false;
        lastUpgradeCheckMs = millis();
        if (shouldUpgrade(networks, entries, lastGoodSsid)) {
            Serial.println("[wifi] a higher-priority network appeared");
            candidates = selectCandidates(networks, entries);
            candidateIndex = 0;
            tryNextCandidate();
        } else {
            enter(LinkState::Connected);
        }
        return;
    }

    candidates = selectCandidates(networks, entries);
    candidateIndex = 0;
    if (candidates.empty()) {
        enterBackoff();
        return;
    }
    tryNextCandidate();
}

}  // namespace

void linkBegin(const std::vector<WifiNetwork>& initial) {
    networks = initial;
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // this module owns retry policy
    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);
    apPassword = deriveApPassword(mac);
    restartSearch();
}

void linkSetNetworks(const std::vector<WifiNetwork>& updated) {
    networks = updated;
    if (state == LinkState::ApFallback) {
        linkStopAp();
    }
    restartSearch();
}

void linkUpdate() {
    switch (state) {
        case LinkState::FastPath:
            if (WiFi.status() == WL_CONNECTED) {
                onConnected();
            } else if (elapsed(stateEnteredMs, kFastPathTimeoutMs)) {
                Serial.println("[wifi] fast path missed, scanning");
                restartSearch();
            }
            break;

        case LinkState::Scanning: {
            int16_t count = WiFi.scanComplete();
            if (count >= 0) {
                handleScanComplete(count);
            } else if (count == WIFI_SCAN_FAILED) {
                enterBackoff();
            }
            break;
        }

        case LinkState::Connecting:
            if (WiFi.status() == WL_CONNECTED) {
                onConnected();
            } else if (elapsed(stateEnteredMs, kConnectTimeoutMs)) {
                Serial.println("[wifi] connect timed out, next candidate");
                ++candidateIndex;
                tryNextCandidate();
            }
            break;

        case LinkState::Connected:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[wifi] connection dropped");
                restartSearch();
                break;
            }
            servicePageScan();
            // Only worth a scan when there is something better to find, and
            // only occasionally: scanning briefly interrupts the station link.
            if (!pageScanRequested && !networks.empty() &&
                lastGoodSsid != networks[0].ssid &&
                dueForUpgradeCheck(millis(), lastUpgradeCheckMs)) {
                upgradeScanInFlight = true;
                startScan();
                enter(LinkState::Scanning);
            }
            break;

        case LinkState::Backoff:
            if (static_cast<int32_t>(millis() - backoffUntilMs) >= 0) {
                startScan();
                enter(LinkState::Scanning);
            }
            break;

        case LinkState::ApFallback:
            // The state machine has nothing to advance here, but the config
            // page still needs a scan to populate its picker -- and this is
            // the state a fresh device is in, where the picker matters most.
            servicePageScan();
            break;
    }
}

LinkStatus linkStatus() {
    LinkStatus status;
    status.state = state;
    if (state == LinkState::Connected) {
        status.ssid = lastGoodSsid;
        status.ip = std::string(WiFi.localIP().toString().c_str());
    }
    return status;
}

bool linkConnected() { return state == LinkState::Connected; }

void linkForceRescan() {
    if (state == LinkState::Backoff) {
        consecutiveFailures = 0;
        restartSearch();
    }
}

void linkStartAp() {
    Serial.printf("[wifi] raising AP %s\n", kApSsid);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(kApSsid, apPassword.c_str());
    enter(LinkState::ApFallback);
}

void linkStopAp() {
    WiFi.softAPdisconnect(/*wifioff=*/true);
    WiFi.mode(WIFI_STA);
}

void linkPrepareForSleep() {
    // ESP-IDF requires the WiFi driver to be stopped before light sleep: the
    // radio is powered down either way, and leaving the driver "started"
    // means the post-wake mode(WIFI_STA) is a no-op and reconnect never
    // recovers. Disconnect alone is not enough -- if it fails, the radio
    // would stay up -- so WIFI_OFF is forced afterwards.
    WiFi.disconnect(/*wifioff=*/false, /*eraseap=*/false);
    WiFi.mode(WIFI_OFF);
}

void linkResumeFromSleep() {
    WiFi.mode(WIFI_STA);
    if (lastGoodSsid.empty()) {
        restartSearch();
        return;
    }
    // Waking in the room you fell asleep in is the overwhelmingly common case,
    // and a scan is ~2s of latency on every single button press. Bet on the
    // last network first and fall back to a scan when the bet is wrong.
    for (const WifiNetwork& network : networks) {
        if (network.ssid == lastGoodSsid) {
            beginConnect(network);
            enter(LinkState::FastPath);
            return;
        }
    }
    restartSearch();
}

const std::string& linkApSsid() {
    static const std::string ssid = kApSsid;
    return ssid;
}

const std::string& linkApPassword() { return apPassword; }

void linkRequestScan() {
    pageScanRequested = true;
    pageScanInFlight = false;
    pageScanResults.clear();
}

std::vector<ScanEntry> linkScanResults() { return pageScanResults; }
```

- [ ] **Step 3: Verify both boards compile**

Run: `cd firmware && pio run -e sticks3 -e feather_s3_revtft`
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add firmware/include/net/wifi_link.h firmware/src/net/wifi_link.cpp
git commit -m "feat(firmware): drive WiFi from a non-blocking state machine"
```

---

### Task 6: Config server

**Files:**
- Create: `firmware/include/net/config_server.h`
- Create: `firmware/src/net/config_server.cpp`

**Interfaces:**
- Consumes: Task 1 (`WifiNetwork`, `mergeWifiNetworkEdits`, `serializeWifiNetworks`), Task 3 (`htmlEscape`), Task 5 (`linkRequestScan`, `linkScanResults`, `linkStatus`, `LinkState`), plus the existing `BusStopConfig`, `buildBusStopList`, `kMaxBusStops`.
- Produces: `struct ConfigState { std::vector<WifiNetwork>* networks; std::vector<BusStopConfig>* stops; bool* alwaysOn; }`; `using ConfigSavedCallback = std::function<void(bool networksChanged)>`; `void configServerBegin(const ConfigState&, const ConfigSavedCallback&)`; `void configServerUpdate()`; `void configServerOnConnected()`; `void configServerUnlock()`; `bool configServerUnlocked()`; `uint32_t configServerUnlockRemainingMs()`; `std::string configServerUrl()`.

**Why the server does not own the data:** `main.cpp` already owns `busStops` and `alwaysOn` and decides when to persist them. The server borrows pointers, exactly as `wifiPortalConnect` took `std::vector<BusStopConfig>*` and `bool*`, and signals through a callback. Keeping persistence in `main.cpp` preserves the existing "only write NVS when the serialized form actually changed" behaviour.

- [ ] **Step 1: Write the header**

Create `firmware/include/net/config_server.h`:

```cpp
// firmware/include/net/config_server.h
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/bus_stop_config.h"
#include "core/wifi_credentials.h"

// The server borrows the caller's state rather than owning it, so main.cpp
// keeps deciding when a change is worth an NVS write.
struct ConfigState {
    std::vector<WifiNetwork>* networks = nullptr;
    std::vector<BusStopConfig>* stops = nullptr;
    bool* alwaysOn = nullptr;
};

// `networksChanged` tells the caller whether the WiFi list moved, so it can
// restart the connection search only when it has to.
using ConfigSavedCallback = std::function<void(bool networksChanged)>;

void configServerBegin(const ConfigState& state,
                       const ConfigSavedCallback& onSaved);
// Serves requests and, in AP mode, the captive-portal DNS. Call every loop.
void configServerUpdate();
// Starts mDNS. Call on each transition into a connected state: the responder
// has to be restarted after the interface comes back.
void configServerOnConnected();

// Opens the five-minute window during which the page is served. The page is
// otherwise locked, because on a shared office LAN anyone could otherwise
// reach it -- physical possession of the device is the gate.
void configServerUnlock();
bool configServerUnlocked();
uint32_t configServerUnlockRemainingMs();

// What to tell the user to type, e.g. "busaunty.local" or "192.168.4.1".
std::string configServerUrl();
```

- [ ] **Step 2: Write the implementation**

Create `firmware/src/net/config_server.cpp`. The page is one form posting to `/save`; rows are numbered so `mergeWifiNetworkEdits` receives them in display order.

```cpp
// firmware/src/net/config_server.cpp
#include "net/config_server.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "core/html_escape.h"
#include "net/wifi_link.h"

namespace {

constexpr char kHostname[] = "busaunty";
constexpr uint16_t kHttpPort = 80;
constexpr uint16_t kDnsPort = 53;
constexpr uint32_t kUnlockWindowMs = 300000;

WebServer server(kHttpPort);
DNSServer dns;
ConfigState state;
ConfigSavedCallback savedCallback;
bool apDnsRunning = false;
bool unlocked = false;
uint32_t unlockedUntilMs = 0;

bool inApMode() { return linkStatus().state == LinkState::ApFallback; }

// The AP is only up when the user has physically asked for it, so it needs no
// second gate; the LAN does.
bool accessAllowed() { return inApMode() || configServerUnlocked(); }

std::string esc(const std::string& raw) { return htmlEscape(raw); }

std::string networkRows() {
    std::string html;
    for (size_t i = 0; i < state.networks->size(); ++i) {
        const WifiNetwork& network = (*state.networks)[i];
        html += "<div class=row>";
        html += "<input name=ssid" + std::to_string(i) +
                " value='" + esc(network.ssid) + "' maxlength=32>";
        // No value attribute: the stored password must never reach the
        // browser. A masked field pre-filled with the real secret looks
        // identical to the user and hands plaintext to anyone reading source.
        html += "<input name=pass" + std::to_string(i) +
                " type=password placeholder='saved - blank keeps it'"
                " maxlength=63>";
        html += "<button name=up value=" + std::to_string(i) + ">&uarr;</button>";
        html += "<button name=down value=" + std::to_string(i) + ">&darr;</button>";
        html += "<button name=del value=" + std::to_string(i) + ">&times;</button>";
        html += "</div>";
    }
    return html;
}

std::string scanOptions() {
    std::string html = "<option value=''>-- pick a visible network --</option>";
    for (const ScanEntry& entry : linkScanResults()) {
        html += "<option value='" + esc(entry.ssid) + "'>" + esc(entry.ssid) +
                " (" + std::to_string(entry.rssi) + ")</option>";
    }
    return html;
}

std::string stopRows() {
    std::string html;
    for (size_t i = 0; i < kMaxBusStops; ++i) {
        const std::string code =
            i < state.stops->size() ? (*state.stops)[i].code : "";
        const std::string name =
            i < state.stops->size() ? (*state.stops)[i].name : "";
        html += "<div class=row>";
        html += "<input name=code" + std::to_string(i) + " value='" +
                esc(code) + "' inputmode=numeric pattern='[0-9]{3,5}'"
                " placeholder='00481' maxlength=5>";
        html += "<input name=name" + std::to_string(i) + " value='" +
                esc(name) + "' placeholder='Home' maxlength=16>";
        html += "</div>";
    }
    return html;
}

void handleRoot() {
    if (!accessAllowed()) {
        server.send(403, "text/html; charset=utf-8",
                    "<h1>Locked</h1><p>Press the config button on the device, "
                    "then reload.</p>");
        return;
    }
    std::string page =
        "<!doctype html><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Bus Aunty</title>"
        "<style>body{font-family:sans-serif;margin:1rem;max-width:34rem}"
        ".row{display:flex;gap:.3rem;margin:.3rem 0}input{flex:1;min-width:0}"
        "h2{margin-top:1.5rem}</style>"
        "<h1>Bus Aunty</h1><form method=post action=/save>"
        "<h2>WiFi networks</h2><p>Tried top to bottom.</p>";
    page += networkRows();
    page += "<h2>Add a network</h2><select name=newssidpick>";
    page += scanOptions();
    page += "</select> <a href=/rescan>rescan</a>";
    page +=
        "<div class=row><input name=newssid placeholder='or type an SSID' "
        "maxlength=32><input name=newpass type=password placeholder='password' "
        "maxlength=63></div>";
    page += "<h2>Bus stops</h2><p>3-5 digits, leading zeros kept.</p>";
    page += stopRows();
    page += "<h2>Power</h2><label><input type=checkbox name=alwayson";
    page += *state.alwaysOn ? " checked" : "";
    page += "> Always on (skip dimming and sleep)</label>";
    page += "<p><button name=save value=1>Save</button></p></form>";
    server.send(200, "text/html; charset=utf-8", page.c_str());
}

void handleRescan() {
    if (!accessAllowed()) {
        server.send(403, "text/plain", "locked");
        return;
    }
    linkRequestScan();
    // A scan takes a couple of seconds and runs asynchronously, so redirecting
    // straight back would render the picker from the results we do not have
    // yet. Hold the user on an interstitial instead of showing an empty list.
    server.send(200, "text/html; charset=utf-8",
                "<!doctype html><meta charset=utf-8>"
                "<meta http-equiv=refresh content='4;url=/'>"
                "<p>Scanning for networks...</p>");
}

std::string arg(const char* name) {
    return std::string(server.arg(name).c_str());
}

void handleSave() {
    if (!accessAllowed()) {
        server.send(403, "text/plain", "locked");
        return;
    }

    std::vector<WifiNetwork> edits;
    for (size_t i = 0; i < state.networks->size(); ++i) {
        WifiNetwork edit;
        edit.ssid = arg(("ssid" + std::to_string(i)).c_str());
        edit.password = arg(("pass" + std::to_string(i)).c_str());
        edits.push_back(edit);
    }

    // Reordering and deletion are submit buttons, so they arrive with the
    // rest of the form and cannot lose a concurrent edit.
    if (server.hasArg("del")) {
        size_t index = static_cast<size_t>(server.arg("del").toInt());
        if (index < edits.size()) {
            edits.erase(edits.begin() + index);
        }
    } else if (server.hasArg("up")) {
        size_t index = static_cast<size_t>(server.arg("up").toInt());
        if (index > 0 && index < edits.size()) {
            std::swap(edits[index - 1], edits[index]);
        }
    } else if (server.hasArg("down")) {
        size_t index = static_cast<size_t>(server.arg("down").toInt());
        if (index + 1 < edits.size()) {
            std::swap(edits[index], edits[index + 1]);
        }
    }

    // The picker wins over the free-text field: it is there precisely so an
    // SSID with a curly apostrophe never has to be typed.
    std::string newSsid = arg("newssidpick");
    if (newSsid.empty()) {
        newSsid = arg("newssid");
    }
    if (!newSsid.empty()) {
        WifiNetwork added;
        added.ssid = newSsid;
        added.password = arg("newpass");
        edits.push_back(added);
    }

    std::vector<WifiNetwork> before = *state.networks;
    *state.networks = mergeWifiNetworkEdits(before, edits);
    bool networksChanged =
        serializeWifiNetworks(before) != serializeWifiNetworks(*state.networks);

    std::vector<BusStopConfig> stopRowsIn;
    for (size_t i = 0; i < kMaxBusStops; ++i) {
        BusStopConfig row;
        row.code = arg(("code" + std::to_string(i)).c_str());
        row.name = arg(("name" + std::to_string(i)).c_str());
        stopRowsIn.push_back(row);
    }
    *state.stops = buildBusStopList(stopRowsIn);
    *state.alwaysOn = server.hasArg("alwayson");

    // Respond before the callback. Saving a network while on the AP makes the
    // caller tear the AP down and go join it, and a response written after
    // that never reaches the browser.
    server.sendHeader("Location", "/");
    server.send(303);

    if (savedCallback) {
        savedCallback(networksChanged);
    }
}

}  // namespace

void configServerBegin(const ConfigState& initial,
                       const ConfigSavedCallback& onSaved) {
    state = initial;
    savedCallback = onSaved;
    server.on("/", handleRoot);
    server.on("/rescan", handleRescan);
    server.on("/save", HTTP_POST, handleSave);
    // In AP mode every probe URL should land on the page, so the phone's
    // captive-portal check opens it automatically.
    server.onNotFound([]() {
        server.sendHeader("Location", "/");
        server.send(303);
    });
    server.begin();
}

void configServerOnConnected() {
    MDNS.end();
    if (MDNS.begin(kHostname)) {
        MDNS.addService("http", "tcp", kHttpPort);
    }
}

void configServerUpdate() {
    if (inApMode()) {
        if (!apDnsRunning) {
            dns.start(kDnsPort, "*", WiFi.softAPIP());
            apDnsRunning = true;
        }
        dns.processNextRequest();
    } else if (apDnsRunning) {
        dns.stop();
        apDnsRunning = false;
    }
    server.handleClient();
}

void configServerUnlock() {
    unlocked = true;
    unlockedUntilMs = millis() + kUnlockWindowMs;
}

bool configServerUnlocked() {
    if (unlocked && static_cast<int32_t>(millis() - unlockedUntilMs) >= 0) {
        unlocked = false;
    }
    return unlocked;
}

uint32_t configServerUnlockRemainingMs() {
    return configServerUnlocked() ? unlockedUntilMs - millis() : 0;
}

std::string configServerUrl() {
    if (inApMode()) {
        return std::string(WiFi.softAPIP().toString().c_str());
    }
    return std::string(kHostname) + ".local";
}
```

- [ ] **Step 3: Verify both boards compile**

Run: `cd firmware && pio run -e sticks3 -e feather_s3_revtft`
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add firmware/include/net/config_server.h firmware/src/net/config_server.cpp
git commit -m "feat(firmware): serve the config page over the LAN behind a button unlock"
```

---

### Task 7: Display screens

**Files:**
- Modify: `firmware/include/ui/display.h`
- Modify: `firmware/src/ui/display.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `void displayShowApSetup(const std::string& ssid, const std::string& password)`; `void displayShowConfigAccess(const std::string& url, uint32_t remainingSec, const std::string& apHint)`; `void displayShowOffline(const std::string& hint)`.

This task only **adds** functions. `displayShowWifiSetup` and `displayShowNoStops` keep working and are removed in Task 8, so the tree compiles at every point.

**Layout note:** `displayShowWifiSetup` centres a block of `icon + gap + 2 lines`. The icon is 68px of ink at `kWifiIconScale = 0.25f`, so ~17px; with `kWifiIconTextGap = 12` and DejaVu18, three lines come to roughly 89px against a 135px panel. It fits on both boards, which share a 240x135 screen.

- [ ] **Step 1: Declare the new screens**

In `firmware/include/ui/display.h`, after `displayShowNoStops`:

```cpp
// The setup AP is WPA2, so the screen has to carry the password: it is
// derived from the chip MAC and therefore differs per device.
void displayShowApSetup(const std::string& ssid, const std::string& password);
// Shown while the LAN config page is unlocked, so the user knows where to
// browse and how long they have.
void displayShowConfigAccess(const std::string& url, uint32_t remainingSec,
                             const std::string& apHint);
// Nothing in range. Distinct from a fetch failure, and it says what to do.
void displayShowOffline(const std::string& hint);
```

- [ ] **Step 2: Implement them**

The AP screen is `displayShowWifiSetup` with a third line, so factor the icon block out rather than duplicating the sprite dance.

Put the helper in the **existing** anonymous namespace, just after the icon constants at `src/ui/display.cpp:51` — not in a second namespace block lower down, or `displayShowWifiSetup` (which Step 2 also rewrites to call it) would reference a function declared after its own definition and fail to compile.

```cpp
// (inside the existing anonymous namespace, after kWifiIconTextGap)

// Draws the WiFi glyph centred above a block of `lineCount` text lines and
// returns the y of the first line. Shared so the AP screen does not have to
// repeat the sprite staging below.
int drawWifiIconBlock(int lineCount) {
    int lineHeight = canvas.fontHeight();
    int iconHeight = static_cast<int>(kWifiIconInkHeight * kWifiIconScale + 0.5f);
    int blockHeight = iconHeight + kWifiIconTextGap + lineHeight * lineCount;
    int iconY = (screenHeight() - blockHeight) / 2;

    // Shrinking the arcs this far needs antialiasing, which only works from a
    // sprite source, so the cropped bitmap is staged before being zoomed down.
    hal::Canvas icon(&canvas);
    icon.setColorDepth(8);
    if (icon.createSprite(WIFIIMAGE_WIDTH, kWifiIconInkHeight)) {
        icon.fillSprite(TFT_BLACK);
        icon.drawBitmap(0, 0,
                        epd_bitmap_WifiImage + kWifiIconInkTop * kWifiIconRowBytes,
                        WIFIIMAGE_WIDTH, kWifiIconInkHeight, TFT_WHITE);
        icon.setPivot(WIFIIMAGE_WIDTH / 2.0f, kWifiIconInkHeight / 2.0f);
        icon.pushRotateZoomWithAA(&canvas, screenWidth() / 2.0f,
                                  iconY + iconHeight / 2.0f, 0.0f,
                                  kWifiIconScale, kWifiIconScale, TFT_BLACK);
        icon.deleteSprite();
    }
    return iconY + iconHeight + kWifiIconTextGap;
}
```

Then add the three screens at file scope, after `displayShowWifiSetup` (`<cstdio>` may need including for `snprintf`):

```cpp
void displayShowApSetup(const std::string& ssid, const std::string& password) {
    canvas.fillSprite(TFT_BLACK);
    // 18px here rather than the shared 16px default. Font 2 is a bitmap font,
    // so scaling it to 18 would resample unevenly; DejaVu18 is natively 18px.
    canvas.setFont(&fonts::DejaVu18);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextDatum(top_center);

    int lineHeight = canvas.fontHeight();
    int textY = drawWifiIconBlock(3);
    canvas.drawString(ssid.c_str(), screenWidth() / 2, textY);
    canvas.drawString(("Pass: " + password).c_str(), screenWidth() / 2,
                      textY + lineHeight);
    canvas.drawString("then open 192.168.4.1", screenWidth() / 2,
                      textY + lineHeight * 2);

    canvas.pushSprite(0, 0);
    canvas.setTextFont(kDefaultTextFont);
}

void displayShowConfigAccess(const std::string& url, uint32_t remainingSec,
                             const std::string& apHint) {
    char countdown[32];
    snprintf(countdown, sizeof(countdown), "Open for %lu:%02lu",
             static_cast<unsigned long>(remainingSec / 60),
             static_cast<unsigned long>(remainingSec % 60));
    displayShowStatus("Config open at\n" + url + "\n" + countdown + "\n" +
                      apHint);
}

void displayShowOffline(const std::string& hint) {
    displayShowStatus("No network\n" + hint);
}
```

Then rewrite the body of the existing `displayShowWifiSetup` to call `drawWifiIconBlock(2)` instead of repeating the sprite code, so there is one copy of it. It is deleted in Task 8 regardless, but leaving two copies in the tree invites the wrong one being edited.

- [ ] **Step 3: Verify both boards compile**

Run: `cd firmware && pio run -e sticks3 -e feather_s3_revtft`
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add firmware/include/ui/display.h firmware/src/ui/display.cpp
git commit -m "feat(firmware): add AP-setup, config-access and offline screens"
```

---

### Task 8: Wire it together and delete WiFiManager

**Files:**
- Modify: `firmware/src/main.cpp`
- Modify: `firmware/platformio.ini:14` (remove `tzapu/WiFiManager@^2.0.17`)
- Modify: `firmware/include/ui/display.h`, `firmware/src/ui/display.cpp` (remove `displayShowWifiSetup`; change `displayShowNoStops` to take a hint string)
- Delete: `firmware/include/net/wifi_portal.h`, `firmware/src/net/wifi_portal.cpp`

**Interfaces:**
- Consumes: everything from Tasks 1–7.
- Produces: nothing new.

This is the task where behaviour changes. Everything before it was additive.

- [ ] **Step 1: Delete the portal and its dependency**

```bash
git rm firmware/include/net/wifi_portal.h firmware/src/net/wifi_portal.cpp
```

In `firmware/platformio.ini`, remove the `tzapu/WiFiManager@^2.0.17` line from `[esp32_base]`'s `lib_deps`, leaving only ArduinoJson.

- [ ] **Step 2: Change `displayShowNoStops` to take a hint**

The old copy said "Hold <button>, then join WiFi: <ssid>", which is no longer how stops are configured. In `display.h`:

```cpp
// `hint` is where to go to add stops -- a URL when connected, the AP name
// when not -- because that now differs by connection state.
void displayShowNoStops(const std::string& hint);
```

In `display.cpp`, replace the `lines` array with:

```cpp
    const std::string holdLine =
        std::string("Hold ") + board().secondaryButtonLabel + ", then";
    const std::string lines[] = {"No bus stops yet", holdLine, "open", hint};
```

Delete `displayShowWifiSetup` from both files.

- [ ] **Step 3: Rewrite the WiFi parts of `main.cpp`**

Replace the `net/wifi_portal.h` include with:

```cpp
#include "net/config_server.h"
#include "net/wifi_link.h"
#include "storage/wifi_store.h"
```

Add to the anonymous namespace, near `busStops`:

```cpp
std::vector<WifiNetwork> wifiNetworks;
// Redraw throttle for the config screen's countdown, which is the only thing
// on screen that changes without an input.
uint32_t lastConfigRenderMs = 0;
constexpr uint32_t kConfigRenderIntervalMs = 1000;
bool showingConfigScreen = false;
```

Replace `onPortalStarted` and `openConfigPortal` with:

Also delete `persistStopsIfChanged` and `persistAlwaysOnIfChanged`. They existed because every boot ran the portal and usually changed nothing; a save now only happens when the user pressed Save, so writing unconditionally is both simpler and correct.

```cpp
// Called by the config server after a successful save. The server owns no
// state, so persistence and re-driving the radio happen here.
void onConfigSaved(bool networksChanged) {
    saveBusStops(busStops);
    saveAlwaysOn(alwaysOn);
    saveWifiNetworks(wifiNetworks);
    if (networksChanged) {
        linkSetNetworks(wifiNetworks);
    }
    if (currentStopIndex >= busStops.size()) {
        currentStopIndex = 0;
    }
    cachedServices.clear();
    currentPage = 0;
    noStopsRendered = false;
    needsImmediateFetch = true;
    lastPollMillis = millis();
}

// Where to send the user to configure the device, which depends on whether
// there is a LAN to reach it on.
std::string configHint() {
    return linkConnected() ? configServerUrl() : linkApSsid();
}

// First press of the gesture opens the LAN page; a second press from that
// screen raises the AP, which is the escape hatch when mDNS will not resolve
// or the office LAN blocks device-to-device traffic.
//
// With no connection there is no LAN page to unlock, so the gesture goes
// straight to the AP rather than showing a URL that cannot be reached.
void openConfigScreen() {
    bool alreadyOnAp = linkStatus().state == LinkState::ApFallback;
    if (!alreadyOnAp && (showingConfigScreen || !linkConnected())) {
        linkStartAp();
    } else if (!alreadyOnAp) {
        configServerUnlock();
    }
    showingConfigScreen = true;
    lastConfigRenderMs = 0;
    noteInteraction();
}

void renderConfigScreen() {
    if (linkStatus().state == LinkState::ApFallback) {
        displayShowApSetup(linkApSsid(), linkApPassword());
        return;
    }
    displayShowConfigAccess(configServerUrl(),
                            configServerUnlockRemainingMs() / 1000,
                            "Press again for AP");
}
```

In `setup()`, replace the `wifiPortalConnect` block with:

```cpp
    busStops = loadBusStops();
    alwaysOn = loadAlwaysOn();

    logWifiDiagnostics();

    WiFi.mode(WIFI_STA);
    wifiNetworks = loadWifiNetworks();
    if (!hasStoredWifiNetworks()) {
        // One shot, guarded by the key's absence so it cannot resurrect a
        // network the user later deleted: carry WiFiManager's credentials
        // over so a flashed device keeps working without re-provisioning.
        WifiNetwork legacy;
        if (importLegacyWifiNetwork(&legacy)) {
            wifiNetworks.push_back(legacy);
            saveWifiNetworks(wifiNetworks);
            Serial.printf("[wifi] imported %s from the old firmware\n",
                          legacy.ssid.c_str());
        }
    }

    displayShowStatus("Connecting WiFi...");
    linkBegin(wifiNetworks);

    ConfigState configState;
    configState.networks = &wifiNetworks;
    configState.stops = &busStops;
    configState.alwaysOn = &alwaysOn;
    configServerBegin(configState, onConfigSaved);

    // No blocking wait and no reboot on failure: a device carried out of
    // range must still come up, show its buttons, and keep looking.
    noteInteraction();
```

Delete the `syncTime()` call from `setup()` and move it into `loop()`, guarded so it runs once per connection — it needs a network, which `setup()` no longer guarantees. Add near the other file-scope state:

```cpp
bool timeSynced = false;
```

In `loop()`, replace the `hal::wasHold(hal::Button::Secondary)` branch body with `openConfigScreen(); return;`, and delete the whole `WiFi.status() != WL_CONNECTED` block.

Its replacement is the ordered sequence below, placed after the button and power handling and before the `busStops.empty()` check. **The order is load-bearing:** the config screen must be reachable while offline, because AP fallback *is* an offline state and is precisely when the user needs that screen. Putting the offline early-return first would make the AP screen unreachable.

```cpp
    linkUpdate();
    configServerUpdate();

    if (showingConfigScreen) {
        if (millis() - lastConfigRenderMs >= kConfigRenderIntervalMs) {
            renderConfigScreen();
            lastConfigRenderMs = millis();
        }
        // Leaves on a deliberate click, or on its own when the unlock window
        // closes. The AP has no window, so it is only left deliberately.
        bool dismissed = hal::wasClicked(hal::Button::Primary);
        bool expired = !configServerUnlocked() &&
                       linkStatus().state != LinkState::ApFallback;
        if (dismissed || expired) {
            if (dismissed && linkStatus().state == LinkState::ApFallback) {
                linkStopAp();
                linkSetNetworks(wifiNetworks);
            }
            showingConfigScreen = false;
            needsImmediateFetch = true;
        }
        delay(50);
        return;
    }

    if (!linkConnected()) {
        timeSynced = false;
        displayShowOffline(linkStatus().state == LinkState::Backoff
                               ? "Press any button to retry"
                               : "Searching...");
        delay(50);
        return;
    }

    if (!timeSynced) {
        // The mDNS responder has to be restarted each time the interface
        // comes back, so this runs on every fresh connection, not just once.
        configServerOnConnected();
        syncTime();
        timeSynced = true;
        needsImmediateFetch = true;
    }
```

In the button-press block near the top of `loop()`, add one line alongside `noteInteraction()`:

```cpp
        noteInteraction();
        // Clears any backoff, so switching a hotspot on and pressing a button
        // is all it takes to be found. A no-op unless actually backing off.
        linkForceRescan();
```

Leave the config screen alone here. It owns its own exit (Step 3's render block below), because the press that *begins* the Secondary hold also fires `wasPressed`, so dismissing the screen from this block would close it on the way into opening it.

In `enterSleep()`, replace the two-line `WiFi.disconnect` / `WiFi.mode(WIFI_OFF)` pair with `linkPrepareForSleep();`, and replace the `WiFi.mode(WIFI_STA); WiFi.begin(); while(...)` block with `linkResumeFromSleep();`. Add `timeSynced = false;` and `showingConfigScreen = false;` to the wake-side resets.

Update `displayShowNoStops(kSetupApSsid)` to `displayShowNoStops(configHint())`, and delete the now-unused `kSetupApSsid` constant.

- [ ] **Step 4: Verify both boards compile**

Run: `cd firmware && pio run -e sticks3 -e feather_s3_revtft`
Expected: SUCCESS, and no reference to `WiFiManager` anywhere.

Run: `rg -i wifimanager firmware/`
Expected: no matches.

- [ ] **Step 5: Verify the host suite still passes**

Run: `cd firmware && pio test -e native`
Expected: all suites pass.

- [ ] **Step 6: Commit**

```bash
git add -A firmware/
git commit -m "feat(firmware): replace WiFiManager with the multi-network link and config server"
```

---

### Task 9: On-device verification and documentation

**Files:**
- Modify: `README.md`

**Interfaces:** none.

- [ ] **Step 1: Work the on-device checklist**

Flash a device and confirm each. Record failures as issues rather than fixing them inline unless they are one-liners.

1. **Migration** — flash over a device running the old firmware. It reconnects to its existing network with no setup.
2. **Fresh provisioning** — erase flash (`pio run -t erase`), boot. The AP screen shows `BusAuntySetup` and an 8-character password. Join it; the page opens by itself. Add a network; the device connects.
3. **Priority** — save two networks, both in range. Confirm it joins the first. Reorder them and confirm it switches after a reboot.
4. **Hotspot** — with *Maximize Compatibility* on, add the phone hotspot as the lowest priority. Out of range of everything else, switch the hotspot on and press a button; the device joins within a few seconds.
5. **Upgrade** — while on the hotspot, bring a higher-priority network into range and wait five minutes. It switches up.
6. **Offline backoff** — carry the device out of range. It shows the offline screen and does not reboot. A button press retries immediately.
7. **Unlock window** — open the config page, wait five minutes, reload. It is locked.
8. **UTF-8 SSID** — add a network whose name has a curly apostrophe using the scan picker. It saves, displays correctly on reload, and connects.
9. **Sleep and wake** — sleep the device and wake it. It reconnects via the fast path in about a second.
10. **Both boards** — repeat 2, 4 and 9 on the other board.

- [ ] **Step 2: Document provisioning and the hotspot gotcha in `README.md`**

Add a WiFi section covering: the `BusAuntySetup` AP and its on-screen password for a new device; `http://busaunty.local` plus the button unlock afterwards; that networks are tried top to bottom; and this, which is the single most likely support question:

> **Phone hotspots must be 2.4 GHz.** The ESP32-S3 has no 5 GHz radio, and recent iPhones default Personal Hotspot to 5 GHz, where the device cannot see it at all. Turn on **Maximize Compatibility** in Settings → Personal Hotspot. iOS also powers the hotspot down when nothing is attached, so switch it on and then press a button on the device to trigger an immediate scan.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "docs: how to add WiFi networks, and why hotspots need 2.4GHz"
```

---

## Notes for the implementer

**Out of scope — do not add these:**
- Hidden SSID support. A scan cannot see them, and the fallback (blind `WiFi.begin()` per network) is deliberately deferred.
- Encrypting stored passwords. Requires partition-level flash encryption.
- HTTPS on the config page.
- Enterprise/802.1X networks.
- OTA updates, despite there now being a web server. The Feather has no OTA partition.
- Relaxing `normalizeBusStopName`'s stripping of `'`, `<` and `>`. It exists because the old portal did not escape; Task 3 makes escaping available, but changing stop-name behaviour is a separate change.

**If you get stuck:** the pure modules (Tasks 1–3) are the contract. If device behaviour surprises you, add a failing host test at the policy level first — it is far cheaper than reflashing.
