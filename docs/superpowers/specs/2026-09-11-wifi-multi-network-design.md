# Multi-Network WiFi (replace WiFiManager) Design

**Goal:** Let the device carry a short, ordered list of WiFi networks — home, phone hotspot, office — and find whichever one is actually there, without a captive portal and without the `tzapu/WiFiManager` dependency. Adding and reordering networks happens on a web page the device serves over your own LAN at `http://busaunty.local`, not over an AP you have to trigger first.

**Architecture:** Five new modules replacing one. The decision-making (which network to try, in what order, how long to wait before retrying) becomes pure logic in `src/core/`, host-testable like `sleep_policy` and `button_gesture` already are. The radio, NVS and HTTP server sit behind thin wrappers in `src/net/` and `src/storage/` that hold no policy of their own.

**Tech Stack:** No new libraries. `WebServer`, `DNSServer`, `ESPmDNS` and `esp_wifi` all ship with the ESP32 Arduino core; `tzapu/WiFiManager` is removed from `lib_deps` in both board envs. ArduinoJson v7 is reused for credential serialization, exactly as `bus_stop_config.cpp` uses it today.

**Validation status:** Nothing here has been compiled or flashed. The WiFiManager credential import (see *Migration*) depends on the ESP-IDF station config being populated from NVS at `esp_wifi_start()`, which is the documented behaviour when `nvs_enable` is set — Arduino sets it — but has not been confirmed on either board. Every item marked **verify on device** is genuinely unverified. Timing constants (scan duration, connect timeout, backoff steps) are estimates to be tuned against real hardware.

---

## Why replace WiFiManager rather than extend it

Four reasons, all of which were given, and all of which point the same way:

1. **It cannot hold more than one network.** This is the actual feature being asked for; the dependency is incidental to it.
2. **It owns code we want to own.** The bus-stop form and the always-on checkbox live inside its parameter system today (`src/net/wifi_portal.cpp`), which is why editing a bus stop means going through a WiFi setup portal.
3. **Its UX fights the device.** `setup()` currently blocks on `wifiPortalConnect()` and reboots after a 180-second timeout, so a device that boots out of range of everything is not merely offline, it is stuck in a reboot loop.
4. **Footprint.** The Feather's 4MB no-OTA partition is already tight enough to need a custom partition table.

The replacement is smaller than WiFiManager because it does less: no captive-portal DNS trickery on the happy path, no parameter abstraction, no template engine.

## Settled decisions

| Question | Decision |
| --- | --- |
| Selection when several saved networks are in range | Priority order set by the user, **not** signal strength |
| Where networks are added | A page the device serves on the LAN, reached by IP or `busaunty.local` |
| First-ever network, when there is no LAN to reach | AP fallback (`BusAuntySetup`), same page served over the AP |
| Nothing in range | Offline screen, backoff 5s → 15s → 60s cap |
| Catching a hotspot switched on mid-commute | Any button press clears the backoff and forces an immediate scan |
| Connected to a low-priority network, better one appears | Rescan every ~5 min while awake, switch up |
| Existing WiFiManager credentials | Imported on first boot as network #1 |
| Setup AP security | WPA2, password derived from the chip MAC, shown on screen |
| LAN page security | Served only during a 5-minute window opened by a button press |
| Saved passwords in the page | Never transmitted; field renders empty, blank means "keep" |
| Config gesture | Hold Secondary → config screen; a further press from there raises the AP |

## Why priority order and not signal strength

`WiFiMulti` — which is in the core and would be nearly free — sorts candidates by RSSI. That is the wrong answer for this device. Standing in the kitchen with a phone hotspot running, RSSI is a coin flip between the hotspot and the house router, and losing that flip means the device quietly spends cellular data while sitting on home WiFi. Priority order is both predictable and cheap to express: the order of the saved list *is* the priority, so there is no separate rank field and no way to reach a state where two networks both claim to be first.

The cost is that we do our own scan-and-pick. That is about thirty lines of pure logic, and it is logic that can be unit-tested on the host, which `WiFiMulti` cannot be.

## Module layout

```
firmware/
  include/
    core/wifi_credentials.h     # NEW pure: WifiNetwork, validation, JSON round-trip
    core/wifi_policy.h          # NEW pure: candidate selection, backoff, upgrade decision
    storage/wifi_store.h        # NEW: NVS load/save
    net/wifi_link.h             # NEW: connection state machine, AP fallback
    net/config_server.h         # NEW: WebServer + mDNS + unlock gate
    net/wifi_portal.h           # DELETED
  src/
    core/wifi_credentials.cpp   # NEW
    core/wifi_policy.cpp        # NEW
    storage/wifi_store.cpp      # NEW
    net/wifi_link.cpp           # NEW
    net/config_server.cpp       # NEW
    net/wifi_portal.cpp         # DELETED
  test/
    test_wifi_credentials/      # NEW
    test_wifi_policy/           # NEW
```

`[env:native]`'s `build_src_filter` already includes `core/` and excludes `net/` and `storage/`, and ArduinoJson is already in its `lib_deps`, so both new core modules are picked up by the host test build with no `platformio.ini` change beyond dropping WiFiManager.

## Data model and storage

```cpp
constexpr size_t kMaxWifiNetworks = 5;

struct WifiNetwork {
    std::string ssid;      // 1-32 bytes, UTF-8, stored verbatim
    std::string password;  // empty (open network) or 8-63 chars
};
```

The vector order is the priority; index 0 is tried first. Serialized as a JSON array into the existing `busaunty` NVS namespace under a `wifi` key, mirroring `bus_stop_store.cpp` including its habit of removing the key rather than storing an empty value, so "no networks configured" stays a single distinguishable state.

SSIDs are stored as raw UTF-8 bytes with no normalization. This is not incidental: an iPhone hotspot is named with a curly apostrophe (U+2019, as in `Lionel’s iPhone`), and any normalization, case-folding or ASCII coercion en route makes that network permanently unjoinable. The form is served with an explicit `charset=utf-8` for the same reason, and the scan-assisted picker (below) exists largely so the SSID never has to be typed at all.

Passwords are stored in plaintext. This matches what ESP-IDF already does with the credentials on the device today; encrypting them would require flash encryption at the partition level, which is out of scope and carries a real bricking risk.

## Connection state machine

WiFi stops being a boot-time blocking step and becomes a state machine that `loop()` advances a little on each pass, so buttons and the screen stay responsive while it works.

| State | Behaviour | Exits to |
| --- | --- | --- |
| `FastPath` | Blind `WiFi.begin(ssid, pass)` on the last network that worked, no scan | `Connected`, or `Scanning` on failure |
| `Scanning` | Async `WiFi.scanNetworks(true)`, ~2s | `Connecting` if any saved network was seen, else `Backoff` |
| `Connecting` | `WiFi.begin()` on the highest-priority network seen, ~10s timeout | `Connected`, next candidate, or `Backoff` when candidates are exhausted |
| `Connected` | Normal operation; rescan every 5 min if not on the top-priority network | `Scanning` on drop or on an upgrade check |
| `Backoff` | Offline screen, wait 5s → 15s → 60s (capped) | `Scanning` on expiry or on any button press |
| `ApFallback` | WPA2 AP + DNSServer + config page | `Scanning` once a network is saved |

Three things worth naming:

**The fast path is not an optimisation, it is the common case.** Waking from light sleep in the same room you fell asleep in is what happens nearly every time, and a full scan costs ~2s of user-visible latency on every single button press. Trying the last-good network directly is ~1s and usually right. `FastPath` is skipped only on cold boot and after an explicit network change.

**`ApFallback` is entered on two conditions**, not one: no networks saved at all (fresh device), or an explicit request from the config screen. It is deliberately *not* entered just because nothing is in range — a device on a bus should sit in `Backoff` showing an offline screen, not raise an AP nobody will connect to.

**Backoff is reset by any button press**, which is the entire mechanism for catching a hotspot. You switch the hotspot on, press a button, the device scans immediately. No polling cost, no guessing at how often to look.

## The hotspot leg, specifically

Three known failure modes, listed because two of them are documentation problems that will otherwise look like firmware bugs:

1. **5 GHz.** The ESP32-S3 radio is 2.4 GHz only. Recent iPhones default Personal Hotspot to 5 GHz, and the device will never see it in a scan. *Maximize Compatibility* in the hotspot settings forces 2.4 GHz. This needs to be in the README — it is the single most likely cause of "the hotspot doesn't work". **Verify on device.**
2. **Intermittent visibility.** iOS only keeps the hotspot radio up while the Personal Hotspot screen is open or a client is attached (roughly a 90-second grace period after the last one leaves). The button-forced rescan is the answer to this; a purely time-based retry would be fighting it.
3. **Hidden networks.** A scan cannot see a hidden SSID, so a hidden Android hotspot would not work. Not handled. The fix, if it is ever needed, is a per-network "hidden" checkbox that falls back to a blind `WiFi.begin()` for that entry alone — deliberately not built now.

## Security model

Three distinct paths a password could travel, and what closes each:

| Exposure | Mitigation |
| --- | --- |
| Saved passwords rendered into the config page | Never sent. The field renders **empty** with a "saved — leave blank to keep" placeholder, and is `type=password` so what you type is masked. A masked field pre-filled with the real value looks identical to the user but hands plaintext to anything reading the page source; that is the version being avoided. |
| A new password typed over an open setup AP | The AP is WPA2. Its password is derived from the chip MAC and displayed on screen when the AP comes up, so it is per-device and needs no lookup. |
| Anyone on a shared LAN reaching the config page | The page is served only during a 5-minute window opened by a button press on the device. Outside that window every route returns a short "press the button on your device" page. Physical possession is the gate. |

The page is plain HTTP. On a WPA2 LAN the link layer already encrypts it; making it properly private would need certificates and is not worth it here.

## Web UI

One page, served identically over LAN and over the AP, three sections:

- **Networks** — the saved list in priority order, each row with up/down/delete. An add form with a dropdown of visible SSIDs plus a free-text field for anything not in range. The dropdown is what lets the iPhone hotspot be added without typing its curly apostrophe. A connected device is not scanning, so the dropdown is filled by an explicit "rescan" button on the page that triggers a scan and re-renders; scanning briefly interrupts the station connection, which is why it is on demand rather than on page load.
- **Bus stops** — the same four code/name rows as the current portal, with the same validation (`normalizeBusStopCode`, 3–5 digits, leading zeros kept). Moved out of WiFiManager wholesale.
- **Always on** — the existing checkbox, with its existing explanation about mains power being indistinguishable from a full battery.

`busaunty.local` via `ESPmDNS`. In `ApFallback` a `DNSServer` wildcard sends the phone's captive-portal probe to the same page, so connecting to the AP pops the page automatically.

## Migration

On first boot under the new firmware, if no `wifi` key exists in NVS, read the station config the ESP-IDF driver loaded from its own NVS partition (`esp_wifi_get_config(WIFI_IF_STA, …)` after `WiFi.mode(WIFI_STA)`) and, if it carries a non-empty SSID, save it as network #1. A flashed device therefore keeps working on home WiFi with no re-provisioning.

This is a one-shot path guarded by the presence of the `wifi` key, so it cannot resurrect a network the user later deleted. **Verify on device** — if the driver turns out not to populate the config from NVS at start, the fallback is to accept one-time re-provisioning through the AP.

## Changes to `main.cpp`

Mostly deletions:

- The `wifiPortalConnect()` call at the end of `setup()`, and the `displayShowStatus("WiFi setup timed out.\nRestarting...")` / `ESP.restart()` that follows it. The device now boots to a usable UI regardless of network state.
- The `WiFi.status() != WL_CONNECTED` block in `loop()` that retries every second — replaced by advancing the state machine.
- `openConfigPortal()` becomes the config screen (URL + unlock countdown, with a further press raising the AP).
- The wake path in `enterSleep()` swaps its blind 20-second `WiFi.begin()` wait for `FastPath`.

`enterSleep()`'s radio teardown is left alone. The comments there about the WiFi driver needing a full stop before light sleep, and `WIFI_OFF` being forced because a failed `disconnect()` would otherwise leave the radio up, record hard-won behaviour and the refactor has no reason to disturb it.

## Testing

Host tests (`[env:native]`, Unity):

- `test_wifi_credentials` — SSID and password length bounds, open networks, `kMaxWifiNetworks` truncation, JSON round-trip, UTF-8 SSID survival (specifically `Lionel’s iPhone`), the "blank password means keep the existing one" merge, and rejection of rows with an empty SSID.
- `test_wifi_policy` — priority ordering against a scan result set, saved-but-not-seen networks excluded, seen-but-not-saved ignored, empty intersection, the backoff schedule including its cap and its reset, and the upgrade decision (fires only below top priority, never when already on #1).

On-device checklist, since the radio and HTTP server cannot be tested off-hardware: fresh-device AP provisioning; migration from an existing WiFiManager install; priority honoured with two networks in range; hotspot caught via forced rescan; upgrade from hotspot to a higher-priority network; offline backoff on a device carried out of range; unlock window expiring; both boards.

## Out of scope

- Hidden SSIDs (see *The hotspot leg*).
- Encrypting stored passwords.
- HTTPS on the config page.
- Enterprise/802.1X networks, which the office may well use — if it does, this design does not cover it and that is a separate piece of work.
- OTA firmware update, even though a web server now exists. The Feather's partition table has no OTA slots.
