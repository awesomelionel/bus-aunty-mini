# Multi-Device Support (add Adafruit ESP32-S3 Reverse TFT Feather) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the firmware build and run on more than one board from one source tree, and add the [Adafruit ESP32-S3 Reverse TFT Feather](https://www.adafruit.com/product/5691) as the second supported board alongside the M5Stack StickS3 — without forking `display.cpp` or `main.cpp` per device.

**Architecture:** Introduce a board layer between the existing pure-logic core and the existing hardware-facing modules. Board-specific code moves into `src/hal/<board>/`, one directory per board, selected at build time by `build_src_filter` in that board's PlatformIO env — so there is exactly one `#ifdef` in the whole tree (a typedef header), not `#ifdef`s scattered through call sites. Everything that is currently device-coupled but is really just arithmetic (screen layout, button gesture timing) moves into `src/core/`, where it becomes host-testable for the first time.

**Tech Stack:** Unchanged for shared code (PlatformIO, Arduino, ArduinoJson v7, WiFiManager, HTTPClient + WiFiClientSecure, `native` + Unity). Per board: M5Unified/M5GFX on the StickS3 (as today); LovyanGFX + Adafruit MAX1704X on the Feather.

**Validation status:** Nothing in this plan was compiled — the session that wrote it had neither PlatformIO nor general network egress. Pin numbers, the PlatformIO board id, and the board's build defaults were read from primary sources ([`platform-espressif32`'s board manifest](https://raw.githubusercontent.com/platformio/platform-espressif32/develop/boards/adafruit_feather_esp32s3_reversetft.json) and [arduino-esp32's `pins_arduino.h` for the variant](https://raw.githubusercontent.com/espressif/arduino-esp32/master/variants/adafruit_feather_esp32s3_reversetft/pins_arduino.h)) rather than from memory. Library version pins below are placeholders to be resolved with `pio pkg` during Task 1. Every item marked **verify on device** is genuinely unverified.

---

## Why a board layer, and the cheaper alternative

If the StickS3 is never used again, the cheapest correct answer is a straight retarget: swap the four device-coupled files for Feather equivalents and delete the StickS3 env. That is roughly half this plan's work and carries no abstraction risk.

This plan does the multi-board version instead, because that is what was asked, and because the abstraction only starts paying for itself at board #3 — the discipline it buys now is that both boards get built in CI (Task 9), so the one you are not holding cannot silently rot.

## What is actually device-coupled today

Only four source files and the build config. `src/core/`, `src/net/` and `src/storage/` are already board-agnostic (`Preferences`, `WiFiManager`, `HTTPClient` are ESP32-generic) and are not touched by this plan.

| File | Coupling |
| --- | --- |
| `firmware/platformio.ini` | one `sticks3` env: board id, 8MB flash, octal PSRAM, `M5Unified` in global `lib_deps` |
| `src/main.cpp` | `M5.begin()`, `M5.update()`, `M5.BtnA`/`M5.BtnB`, `setHoldThresh` |
| `src/ui/display.cpp` | `M5Canvas`/`M5.Display`, `setRotation(1)`, hardcoded `240`/`135`, hardcoded `kEtaColX`, `kServicesPerScreen = 6`, brightness levels, `"Hold Btn B"` in `displayShowNoStops` |
| `src/ui/battery.cpp` | `M5.Power.getBatteryLevel()`, `M5.Power.isCharging()`, and the `percent < 0` ⇒ "no battery" convention |
| `src/power/sleep.cpp` | `GPIO_NUM_11`/`GPIO_NUM_12`, `GPIO_INTR_LOW_LEVEL` (assumes both buttons are active-low), `M5.update()` in the release wait |

## The two boards, side by side

| | M5Stack StickS3 | Adafruit ESP32-S3 Reverse TFT Feather |
| --- | --- | --- |
| PlatformIO board | `esp32-s3-devkitc-1` + `board_build.*` overrides | `adafruit_feather_esp32s3_reversetft` (real board id) |
| Flash / PSRAM | 8MB / 8MB octal (`qio_opi`) | 4MB / 2MB quad (`qio_qspi`, `BOARD_HAS_PSRAM` already in the manifest) |
| Partitions | `default_8MB.csv` | `partitions-4MB-tinyuf2.csv` (manifest default — leave it alone) |
| Panel | ST7789P3, 135x240 native, `setRotation(1)` | ST7789, 135x240 native, SPI: CS 42, DC 40, RST 41, SCK 36, MOSI 35 |
| Backlight | `M5.Display.setBrightness()` | `TFT_BACKLITE` = GPIO 45, via LEDC PWM |
| Rail gotcha | none | `TFT_I2C_POWER` = **GPIO 7** must be driven HIGH before the panel *or* I2C works (this is *not* GPIO 21 — 21 is `NEOPIXEL_POWER`) |
| Buttons | KEY1 GPIO 11, KEY2 GPIO 12 — both **active-low** with pull-ups | D0 GPIO 0 **active-low** (also BOOT), D1 GPIO 1 and D2 GPIO 2 **active-high**, needing `INPUT_PULLDOWN` |
| Button library | `M5.BtnA`/`M5.BtnB` (`wasClicked`/`wasHold`) | none — we supply the gesture logic |
| Battery | PMIC voltage, no fuel gauge; reports < 0 with no battery | MAX17048 fuel gauge over I2C (`cellPercent()`, `chargeRate()`); **cannot tell that no battery is attached** |
| USB | `-DARDUINO_USB_MODE=1` (hardware CDC) | manifest sets `ARDUINO_USB_CDC_ON_BOOT=1` and TinyUSB; **do not** force `ARDUINO_USB_MODE=1` here |

Two consequences worth naming before any code is written:

1. **Same resolution, so nothing visual has to change** — both panels are 240x135 in landscape. The layout refactor in Task 3 is therefore provably behaviour-preserving (its tests assert the current pixel columns), and the Feather gets the screen you already have.
2. **Battery presence is undetectable on the Feather**, and `main.cpp`'s sleep gate depends on it (`inputs.externallyPowered = battery.charging || battery.percent < 0`). With no battery, a MAX17048 reads the charger rail and reports a healthy ~100%, so the Feather would doze off on USB. Task 7 handles this; see also *Unresolved Questions*.

## Global constraints

- One binary per board. No runtime board detection, no virtual dispatch: the libraries differ per board, so a fat binary would buy nothing and cost flash (the Feather has 4MB).
- Board selection is by directory, not by `#ifdef`. Exactly one file (`include/hal/gfx_types.h`) is allowed a board `#if`, because a shared include path cannot resolve to two different headers any other way.
- Pure logic keeps going to `src/core/` and gains tests. Nothing moves *into* `src/hal/` that could be tested on the host.
- No behaviour change on the StickS3. Tasks 1–5 are a refactor; the StickS3 must look and feel identical after them, and the layout tests exist to prove it.
- Public API of the shared modules (`displayShow*`, `batteryReading`, `powerSleepUntilButtonPress`) changes only where a board genuinely differs.

---

## Target layout

```
firmware/
  include/
    board/board.h                 # BoardProfile + board() accessor
    core/layout.h                 # NEW pure: screen geometry -> pixel positions
    core/button_gesture.h         # NEW pure: (pressed, nowMs) -> click/hold events
    hal/gfx_types.h               # the one #ifdef: Gfx/Canvas typedefs per board
    hal/display_device.h          # panel bring-up, brightness, sleep/wake
    hal/buttons.h                 # logical buttons, board-agnostic
    hal/power.h                   # PowerStatus (replaces ui/battery.h)
    hal/sleep.h                   # moved from power/sleep.h
    net/…  storage/…  ui/display.h   # unchanged headers
  src/
    core/layout.cpp  core/button_gesture.cpp     # NEW, host-tested
    hal/sticks3/{display_device,buttons,power,sleep,board}.cpp
    hal/feather_s3_revtft/{display_device,buttons,power,sleep,board}.cpp
    ui/display.cpp                # board-free after Task 4
    main.cpp                      # board-free after Task 5
  test/
    test_layout/  test_button_gesture/           # NEW
```

`BoardProfile` is what lets one `main.cpp` and one `display.cpp` serve both boards:

```cpp
// include/board/board.h
struct BoardProfile {
    const char* name;              // "StickS3", "Reverse TFT Feather"
    int screenWidth, screenHeight; // 240, 135 on both — read, don't assume
    uint8_t rotation;
    uint8_t brightnessFull, brightnessDim;
    // What to call the buttons on screen: "Btn A"/"Btn B" vs "D1"/"D2".
    const char* primaryButtonLabel;
    const char* secondaryButtonLabel;
    bool hasSleepButton;           // Feather's D0 sleeps on a single press
    // False when the board cannot distinguish an absent battery from a full
    // one, so the sleep gate must not read percent < 0 as "on USB".
    bool detectsBatteryPresence;
};
const BoardProfile& board();
```

---

### Task 1: Restructure `platformio.ini` for multiple envs

**Files:** modify `firmware/platformio.ini`

**Interfaces:** produces envs `sticks3`, `feather_s3_revtft`, `native`, and the `BOARD_*` defines every later task selects on.

- [ ] **Step 1:** Factor the shared ESP32 settings into an `[esp32_base]` section and have both board envs `extends` it. Move `m5stack/M5Unified` out of the shared `lib_deps` into the `sticks3` env only — the Feather build must not pull M5GFX in (see the symbol-clash note in Task 4).

```ini
[platformio]
default_envs = sticks3, feather_s3_revtft

[esp32_base]
platform = espressif32
framework = arduino
monitor_speed = 115200
lib_deps =
    bblanchon/ArduinoJson@^7.2.2
    tzapu/WiFiManager@^2.0.17

[env:sticks3]
extends = esp32_base
board = esp32-s3-devkitc-1
board_build.arduino.memory_type = qio_opi
board_build.psram_type = opi
board_upload.flash_size = 8MB
board_build.partitions = default_8MB.csv
build_flags =
    -DBOARD_STICKS3
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
build_src_filter = +<*> -<hal/feather_s3_revtft/>
lib_deps =
    ${esp32_base.lib_deps}
    m5stack/M5Unified@^0.2.21

[env:feather_s3_revtft]
extends = esp32_base
board = adafruit_feather_esp32s3_reversetft
build_flags =
    -DBOARD_FEATHER_S3_REVTFT
build_src_filter = +<*> -<hal/sticks3/>
lib_deps =
    ${esp32_base.lib_deps}
    lovyan03/LovyanGFX          ; pin the resolved version
    adafruit/Adafruit MAX1704X  ; pin the resolved version
```

The Feather env deliberately sets no memory/flash/partition/USB flags: its manifest already carries `qio_qspi`, `BOARD_HAS_PSRAM`, `partitions-4MB-tinyuf2.csv` and `ARDUINO_USB_CDC_ON_BOOT=1`, and overriding `ARDUINO_USB_MODE` would fight the TinyUF2 bootloader.

- [ ] **Step 2:** Extend the `native` env's `build_src_filter` with `-<hal/>` so no board code reaches the host build, and drop the now-redundant `-<power/>` once `power/sleep.cpp` moves in Task 6.
- [ ] **Step 3:** Resolve and pin the two new library versions (`pio pkg install -e feather_s3_revtft`, then write the resolved versions into `lib_deps` with `@^x.y.z`).
- [ ] **Step 4:** `pio run -e sticks3` still builds; `pio test -e native` still passes (12 tests). The Feather env will not build yet — that is expected until Task 7.

---

### Task 2: Add the board profile

**Files:** create `firmware/include/board/board.h`, `firmware/src/hal/sticks3/board.cpp`, `firmware/src/hal/feather_s3_revtft/board.cpp`

- [ ] **Step 1:** Define `BoardProfile` and `board()` as sketched above.
- [ ] **Step 2:** Fill in the StickS3 profile with the values currently hardcoded in `display.cpp` — `240`, `135`, rotation `1`, brightness `128`/`16`, labels `"Btn A"`/`"Btn B"`, `hasSleepButton = false`, `detectsBatteryPresence = true`.
- [ ] **Step 3:** Fill in the Feather profile: `240`, `135`, rotation **verify on device** (pick whichever of 1/3 puts the three buttons along the bottom edge — the panel is mounted "reversed", so the StickS3's `1` may well come out upside down), same brightness levels to start, labels `"D1"`/`"D2"`, `hasSleepButton = true`, `detectsBatteryPresence = false`.

---

### Task 3: Extract screen layout into pure, tested logic

**Files:** create `firmware/include/core/layout.h`, `firmware/src/core/layout.cpp`, `firmware/test/test_layout/test_layout.cpp`; modify `firmware/src/ui/display.cpp`

**Interfaces:** produces `computeArrivalsLayout()`, consumed by `display.cpp` (Task 4) and by `main.cpp` via `servicesPerScreen`.

This is the task that makes multi-device support real rather than nominal: today `kServicesPerScreen = 6` and `kEtaColX = {119, 178, 236}` are magic numbers that happen to suit 240x135, and `kServicesPerScreen` leaks out of `display.h` into `main.cpp`'s paging.

- [ ] **Step 1:** Define the pure interface. Row height is passed in rather than measured here, because measuring needs a font and a font needs a device.

```cpp
struct ArrivalsLayout {
    size_t servicesPerScreen;   // rows that fit under the header
    int rowHeight;
    int serviceColX;
    int etaColRightX[kArrivalsPerService];
    int headerCenterX;          // centred in what the battery icon leaves free
    int pageDotsY;
};
ArrivalsLayout computeArrivalsLayout(int screenWidth, int screenHeight,
                                     int rowHeight, int batteryReservedWidth);
```

- [ ] **Step 2:** Derive, don't hardcode: `servicesPerScreen = screenHeight / rowHeight - 1`; the ETA columns are `kArrivalsPerService` right-aligned columns of equal pitch ending 4px from the right edge; `pageDotsY = screenHeight - 4`.
- [ ] **Step 3:** Write the tests. The **critical assertion** is that `computeArrivalsLayout(240, 135, 18, …)` reproduces exactly today's values — `servicesPerScreen == 6`, `etaColRightX == {119, 178, 236}`, `pageDotsY == 131` — so the refactor is provably a no-op on the StickS3. Add a second case at a different geometry (e.g. 320x240 @ 18px ⇒ 12 rows) to prove nothing is silently pinned to 240x135, and a degenerate case (`rowHeight` taller than the screen ⇒ `servicesPerScreen == 0`, no division by zero).
- [ ] **Step 4:** Replace `kServicesPerScreen` in `display.h` with a runtime `size_t servicesPerScreen()` so `main.cpp`'s paging asks the layout instead of a constant.
- [ ] **Step 5:** `pio test -e native` — new tests pass; `pio run -e sticks3` builds; **verify on device:** the arrivals screen is pixel-identical to before, including the page dots and the two-pass bold on `Arr`.

---

### Task 4: Make `display.cpp` board-free

**Files:** create `firmware/include/hal/gfx_types.h`, `firmware/include/hal/display_device.h`, `firmware/src/hal/sticks3/display_device.cpp`; modify `firmware/src/ui/display.cpp`

The trick here is that M5GFX *is* a fork of LovyanGFX, with the same `LGFX_Sprite`, the same `fonts::DejaVu18`, the same `drawString`/`setTextDatum`/`pushRotateZoomWithAA`, and the same `TFT_*` colour macros. So `display.cpp` does not need rewriting — it needs its two concrete types aliased, and its panel bring-up moved out.

- [ ] **Step 1:** `include/hal/gfx_types.h` — the single permitted `#ifdef`:

```cpp
#pragma once
#if defined(BOARD_STICKS3)
#include <M5Unified.h>
namespace hal { using Gfx = M5GFX; using Canvas = M5Canvas; }
#elif defined(BOARD_FEATHER_S3_REVTFT)
#include <LovyanGFX.hpp>
namespace hal { using Gfx = LGFX_Device; using Canvas = LGFX_Sprite; }
#else
#error "No BOARD_* define; see platformio.ini"
#endif
```

- [ ] **Step 2:** `include/hal/display_device.h` — `hal::Gfx& gfx();`, `void displayDeviceBegin();` (power rails, panel init, rotation, initial brightness), `void displayDeviceSetBrightness(uint8_t);`, `void displayDeviceSleep();`, `void displayDeviceWake();`.
- [ ] **Step 3:** StickS3 implementation: `gfx()` returns `M5.Display`; `displayDeviceBegin()` does the `setRotation`/`setBrightness` currently in `displaySetup()`; sleep/wake wrap `M5.Display.sleep()/wakeup()` keeping the existing comment about why brightness is restored rather than zeroed.
- [ ] **Step 4:** In `display.cpp`, replace `M5Canvas canvas(&M5.Display)` with a `hal::Canvas` bound to `hal::gfx()`, drop `#include <M5Unified.h>`, take every geometry constant from `board()` and `computeArrivalsLayout()`, and take the button name in `displayShowNoStops` from `board().secondaryButtonLabel` instead of the literal `"Hold Btn B"`.
- [ ] **Step 5:** `pio run -e sticks3`; **verify on device:** every screen (status, WiFi setup with the scaled antialiased icon, no-stops, arrivals, dim, sleep, wake) is unchanged.

**Risk and fallback:** if the two GFX forks turn out to have drifted on any call `display.cpp` uses, the fallback is a thin `hal::Surface` wrapper exposing the dozen primitives the UI actually needs, implemented per board. That is more code and loses direct font access, so try the typedef first — and build the Feather env early (Task 7 Step 1 is a deliberate spike) rather than discovering drift at the end.

---

### Task 5: Make `main.cpp` board-free

**Files:** create `firmware/include/core/button_gesture.h`, `firmware/src/core/button_gesture.cpp`, `firmware/test/test_button_gesture/test_button_gesture.cpp`, `firmware/include/hal/buttons.h`, `firmware/src/hal/sticks3/buttons.cpp`; modify `firmware/src/main.cpp`

`main.cpp` currently speaks `M5.BtnA`/`M5.BtnB` directly. The Feather has no button library at all, so rather than write one per board, put the gesture state machine in `core/` where both boards share it — and where it can finally be tested.

- [ ] **Step 1:** `core/button_gesture.h` — a `ButtonGesture` fed `update(bool physicallyPressed, uint32_t nowMs)`, exposing `isPressed()`, `wasPressed()`, `wasClicked()`, `wasHold()`, with a configurable hold threshold and ~20ms debounce. Reproduce M5Unified's semantics exactly, because `main.cpp` depends on them: `wasHold()` fires **once** when the threshold is crossed while still held, and `wasClicked()` fires on release **only if** the hold never fired.
- [ ] **Step 2:** Tests for: debounce swallowing a 5ms blip; press→release under threshold ⇒ one `wasClicked`, no `wasHold`; press held past threshold ⇒ one `wasHold`, and no `wasClicked` on the eventual release; `wasHold` not firing twice during one long hold; `millis()` rollover (`nowMs` wrapping past 2^32) not producing a spurious hold.
- [ ] **Step 3:** `include/hal/buttons.h` — logical buttons, not physical ones:

```cpp
namespace hal {
enum class Button { Primary, Secondary, Sleep };  // Sleep may be absent
void buttonsBegin();
void buttonsUpdate();                 // read pins, feed the gestures
bool wasPressed(Button), wasClicked(Button), wasHold(Button), isPressed(Button);
bool hasButton(Button);
}
```

- [ ] **Step 4:** StickS3 implementation: `Primary` ← `M5.BtnA`, `Secondary` ← `M5.BtnB`, `Sleep` absent. Feed `ButtonGesture` from `M5.BtnX.isPressed()` rather than using M5's own click/hold state, so both boards share one tested gesture path.
- [ ] **Step 5:** Rewrite `main.cpp`'s input against `hal::` — `M5.begin()` becomes `displayDeviceBegin()` + `buttonsBegin()`, `M5.update()` becomes `buttonsUpdate()`, and the `Sleep` button is honoured when `hasButton(Button::Sleep)` (hold-on-Primary stays, so the StickS3 is unaffected).
- [ ] **Step 6:** `pio test -e native`; `pio run -e sticks3`; **verify on device:** click pages, click at the last page advances the stop, hold Primary 1.5s sleeps, hold Secondary 3s opens the portal, a wake press does nothing else.

---

### Task 6: Split power and sleep behind the HAL

**Files:** create `firmware/include/hal/power.h`, `firmware/include/hal/sleep.h`, `firmware/src/hal/sticks3/power.cpp`, `firmware/src/hal/sticks3/sleep.cpp`; delete `firmware/include/ui/battery.h`, `firmware/src/ui/battery.cpp`, `firmware/include/power/sleep.h`, `firmware/src/power/sleep.cpp`; modify `firmware/src/main.cpp`, `firmware/src/ui/display.cpp`

- [ ] **Step 1:** Replace `BatteryReading` with a status that says what it knows, so `main.cpp` stops inferring external power from a sentinel:

```cpp
struct PowerStatus {
    int percent = -1;             // -1 when unknown
    bool charging = false;
    bool externalPower = false;   // board-derived, not inferred by the caller
};
void powerBegin(); void powerPoll(); PowerStatus powerStatus();
```

- [ ] **Step 2:** StickS3 implementation: the existing 2s sampling and 0.2 smoothing, verbatim; `externalPower = charging || percent < 0`, with the existing comment about a fully-charged device on USB reporting neither, moved here from `main.cpp`.
- [ ] **Step 3:** Move `powerSleepUntilButtonPress()` to `hal/sleep.h`, and have the StickS3 implementation take its wake pins and polarity from a board-local table instead of the file-scope `kButtonPins`, and its release-wait from `hal::isPressed()` instead of `M5.update()`.
- [ ] **Step 4:** In `main.cpp`, `inputs.externallyPowered = powerStatus().externalPower`. In `display.cpp`, `drawBattery` takes a `PowerStatus` and skips drawing when `percent < 0`.
- [ ] **Step 5:** `pio run -e sticks3`; **verify on device:** the battery icon still tracks, still greens while charging and reds at ≤20%; the two idle stages and hold-to-sleep are unchanged; nothing sleeps on USB with the battery charging.

At this point the StickS3 is fully refactored, the tree has a board layer with one board in it, and nothing about the device's behaviour has changed. Everything after this is additive.

---

### Task 7: Feather — display bring-up

**Files:** create `firmware/src/hal/feather_s3_revtft/display_device.cpp`

- [ ] **Step 1 (spike first):** Before writing the panel config, get `pio run -e feather_s3_revtft` to compile with stub HAL implementations. This is the earliest point at which the Task 4 typedef gamble either holds or doesn't, and it is much cheaper to learn now.
- [ ] **Step 2:** Drive `TFT_I2C_POWER` (**GPIO 7**) HIGH and give the rail a few ms to settle before touching the panel or I2C. Nothing works without this, and the failure mode is a blank screen with no error. Leave `NEOPIXEL_POWER` (GPIO 21) LOW — the NeoPixel is unused and its rail costs current.
- [ ] **Step 3:** Configure a LovyanGFX device: `Bus_SPI` on SPI2/3 with SCK 36, MOSI 35, CS 42, DC 40, RST 41 (MISO unused, `-1`), 40MHz write clock to start; `Panel_ST7789` with `panel_width = 135`, `panel_height = 240`, `offset_x = 52`, `offset_y = 40`, `invert = true`, `rgb_order = false`; `Light_PWM` on `pin_bl = 45` so `setBrightness()` keeps working exactly as it does on the StickS3.
- [ ] **Step 4 (verify on device):** the panel offsets are the one thing most likely to be wrong. If the image is shifted by a few pixels or wraps, try `offset_x = 40, offset_y = 53` (the other common mapping for this 1.14" module) and the `offset_rotation` field before touching anything else. Then settle `board().rotation` so the three buttons sit along the bottom edge.
- [ ] **Step 5:** Implement sleep/wake with LovyanGFX's `sleep()`/`wakeup()`, mirroring the StickS3's "clear the sprite before sleeping, restore full brightness on wake" behaviour.
- [ ] **Step 6 (verify on device):** every screen renders — status text centred, the antialiased WiFi icon (this exercises `pushRotateZoomWithAA` on the LovyanGFX side), no-stops naming `D2`, a full arrivals screen with page dots.

---

### Task 8: Feather — buttons, power, sleep

**Files:** create `firmware/src/hal/feather_s3_revtft/buttons.cpp`, `.../power.cpp`, `.../sleep.cpp`

- [ ] **Step 1:** Buttons: `Primary` = D1 (GPIO 1, `INPUT_PULLDOWN`, **active-high**), `Secondary` = D2 (GPIO 2, `INPUT_PULLDOWN`, **active-high**), `Sleep` = D0 (GPIO 0, `INPUT_PULLUP`, **active-low**). Normalise polarity per pin before feeding `ButtonGesture`, so the shared gesture logic never learns about it. Give D0 **no hold gesture** — it is the BOOT pin, and holding it across a reset drops the board into the ROM bootloader.
- [ ] **Step 2 (verify on device):** all three buttons register, none is inverted, and D0's single press sleeps immediately.
- [ ] **Step 3:** Power: read the MAX17048 over I2C (SDA 3, SCL 4, after the GPIO 7 rail is up) via `Adafruit_MAX1704X` — `cellPercent()` clamped to 0..100, `chargeRate() > 0` ⇒ `charging`. Keep the same 2s sampling and smoothing as the StickS3 for a consistent icon, even though a fuel gauge is steadier than a voltage divider. If `begin()` fails, report `percent = -1` so `drawBattery` simply omits the icon rather than lying.
- [ ] **Step 4:** `externalPower = charging` only. Do **not** map "high percentage" to "no battery": on this board they are indistinguishable, which is why `detectsBatteryPresence` is false in the profile. See *Unresolved Questions* — this is the one place where the Feather is behaviourally worse than the StickS3 and it needs a decision, not a guess.
- [ ] **Step 5:** Sleep: light sleep with per-pin wake polarity — `GPIO_INTR_LOW_LEVEL` for D0 with `gpio_pullup_en`, `GPIO_INTR_HIGH_LEVEL` for D1/D2 with `gpio_pulldown_en`. Keep the existing pre-sleep release wait (the sleep button is usually still down) and the post-wake swallow. Leave the GPIO 7 rail powered through sleep for now: cutting it saves more but costs a fuel-gauge re-init on every wake — note it as a follow-up, don't do it here.
- [ ] **Step 6 (verify on device, the full sleep matrix):** dim at 30s; sleep at 2 min; each of the three buttons wakes it; the wake press does not also page or change stop; after wake the clock is still right (light sleep, so no NTP re-sync) and the stops are still loaded; on USB while charging it never sleeps.
- [ ] **Step 7:** End-to-end on the Feather: captive portal, save WiFi + stops, NTP sync, live arrivals for a real stop code, paging, stop cycling, reopening the portal on `D2` hold, and a check that the 4MB partition still has headroom (`pio run -e feather_s3_revtft` flash usage).

---

### Task 9: CI and docs

**Files:** create `.github/workflows/firmware.yml`; modify `README.md`

- [ ] **Step 1:** A workflow that runs `pio test -e native` and then `pio run` (which builds both `default_envs`). This is the whole point of keeping two boards: without it, the board you are not holding breaks silently. The repo has no CI today, so this is new.
- [ ] **Step 2:** README: turn the single "Hardware" section into a supported-board table, give each board its own build/flash command (`pio run -e feather_s3_revtft -t upload`), and give each board its own button-map table — the current one names KEY1/KEY2, which means nothing on a Feather. Note the Feather's UF2 double-tap-reset upload path alongside the StickS3's plain esptool one.
- [ ] **Step 3:** README: document what is board-specific in one short "Adding a board" paragraph — a `BoardProfile`, four files under `src/hal/<board>/`, one env in `platformio.ini`, and a `-DBOARD_*` define — so board #3 doesn't have to be reverse-engineered from this plan.
- [ ] **Step 4:** Correct the two README claims that stop being true: the battery section's "comes from the PMIC's voltage reading" is StickS3-only, and the sleep section's "no battery being attached" heuristic does not hold on the Feather.

---

## Unresolved Questions

- **Sleeping on USB (the one real regression):** with no battery attached, the Feather's MAX17048 reads the charger rail and reports a healthy charge, so `externalPower` can only come from `chargeRate() > 0`. A Feather on USB with no battery, or with a *fully charged* one, will therefore dim and sleep where a StickS3 would stay lit. Options: (a) accept it — one press brings it back, and it is the same imperfection the README already documents for a fully-charged StickS3; (b) add a portal setting "always-on (mains powered)" that disables the idle stages entirely, which is honest about the fact that this is a deployment choice rather than something the hardware can sense; (c) solder a VBUS sense wire. I'd take (b) — it is ~20 lines given the portal already exists, and it makes the always-on desk-clock case explicit on both boards. Which do you want?
- **Keep the StickS3 at all?** Everything in Tasks 1–6 is refactoring you only need if the StickS3 stays supported. If it is going in a drawer, say so and this plan collapses to Tasks 7–9 against a directly-retargeted tree, which is roughly half the work.
- **Use the Feather's third button for something better than sleep?** This plan assigns D0 to sleep-now, which frees the Primary hold gesture. It could instead be "force refresh now" or "jump to stop 1". Sleep is the conservative default because it keeps the two boards' gesture sets aligned.
- **Rotation and offsets are unverified.** Task 7 assumes the 240x135 landscape orientation carries over and gives fallback offsets to try. If the panel comes up shifted or mirrored, that is expected — it is a fifteen-minute fiddle on device, not a design problem, and no other task depends on the outcome.
- **LovyanGFX vs M5GFX API drift.** Task 4's typedef assumes the two forks still agree on the ~14 calls `display.cpp` makes. Task 7 Step 1 is the spike that proves it. If it fails, the fallback (a thin per-board `Surface` wrapper) adds maybe half a day and does not change any other task.
