# Bus Aunty Display Firmware

ESP32-S3 firmware that polls
`GET https://api.busaunty.com/api/v1/BusArrival?BusStopCode=<code>` every 30
seconds and shows up to 6 bus services, with all 3 arrival times each, for up
to 4 configured bus stops. Bus stops and WiFi credentials are both set on a
page the device serves at `http://busaunty.local` — nothing is compiled in.

## Supported boards

One source tree builds for both boards below; pick one with `-e <env>`.

| | `sticks3` | `feather_s3_revtft` |
| --- | --- | --- |
| Board | [M5Stack StickS3](https://docs.m5stack.com/en/core/StickS3) | [Adafruit ESP32-S3 Reverse TFT Feather](https://www.adafruit.com/product/5691) |
| SoC | ESP32-S3-PICO-1-N8R8 | ESP32-S3 |
| Flash / PSRAM | 8MB / 8MB octal | 4MB / 2MB quad |
| Display | 135x240 ST7789P3 | 135x240 ST7789 |
| Battery gauge | PMIC voltage reading | MAX17048 fuel gauge |
| Buttons | 2 | 3 |
| Upload | esptool over USB-C | UF2 or esptool over USB-C |

Both panels are driven in 240x135 landscape, so the two devices show exactly
the same screens. A USB-C cable is needed for flashing and power; the battery
is optional on both, though only the StickS3 can tell that it is missing (see
[Sleep mode](#sleep-mode)).

The two boards have different numbers of buttons, so they get different
layouts. The Feather's three sit in a column beside the screen and are used as
up / middle / down:

| Feather | Gesture | Action |
| --- | --- | --- |
| D2, top | press | Forward: next page of services, then on to the next stop |
| D0, bottom | press | Back the same way: previous page, then the previous stop |
| D1, middle | press | Sleep now |
| D1, middle | hold 3s | Unlock the config page at `busaunty.local` |
| any | press | Wake from sleep |

The StickS3 has only two buttons, so it keeps the original single-direction
layout:

| StickS3 | Gesture | Action |
| --- | --- | --- |
| KEY1, front | press | Forward: next page of services, then on to the next stop |
| KEY1, front | hold 1.5s | Sleep now |
| KEY2, side | hold 3s | Unlock the config page at `busaunty.local` |
| either | press | Wake from sleep |

Both wrap around, so a short list of stops is a loop rather than a dead end,
and landing on a new stop always starts at its first page.

Two details specific to the Feather. Its D0/D1/D2 silkscreen is on the *back*
of the board, so "hold D1" would tell a user looking at the screen nothing —
the config hold is on the middle button precisely because that one can be described
by position, and the no-stops screen asks for "the middle btn". And D0 is also
the BOOT pin, which is harmless here: the ROM bootloader is only entered by
holding D0 *across a reset*, and D0 carries no hold gesture anyway.

## Prerequisites

- Python 3 and pip.
- PlatformIO Core: `pip install platformio` (adds a `pio` command; on macOS/Linux
  it's usually installed to `~/.local/bin` or `~/Library/Python/<ver>/bin` —
  add that to your `PATH`, or call it via its full path).

## Build, flash, and monitor

```bash
cd firmware
pio run                                  # build both boards
pio run -e sticks3                       # build one
pio run -e sticks3 -t upload             # build + flash over USB-C
pio run -e sticks3 -t upload -t monitor  # build + flash + open serial monitor
```

Swap in `-e feather_s3_revtft` for the Feather:

```bash
pio run -e feather_s3_revtft -t upload -t monitor
```

The StickS3 flashes with plain esptool and needs nothing special. The Feather
ships the TinyUF2 bootloader, so `-t upload` works the same way, but if the
port does not appear (or a bad build has left it unresponsive) double-tap the
reset button to mount it as a `FTHRS3BOOT` drive and copy
`.pio/build/feather_s3_revtft/firmware.uf2` onto it.

The Feather's default partition table splits its 4MB into two 1408K OTA slots,
which this firmware does not fit in. The `feather_s3_revtft` env therefore
selects `tinyuf2-partitions-4MB-noota.csv`, giving the app 2816K and keeping
the `uf2` partition so double-tap flashing still works.

## Run the unit tests

The pure logic in `src/core/` (ISO-8601 parsing, ETA formatting, JSON parsing,
bus stop config validation, WiFi credential and connection policy, HTML
escaping, the sleep/dim decision, the screen layout, the button click/hold
gestures) runs as host-native unit tests — no device needed:

```bash
cd firmware
pio test -e native
```

The `native` environment excludes `main.cpp`, `src/hal/`, `src/net/`,
`src/storage/` and `src/ui/`, so WiFi provisioning, HTTPS fetch, NVS storage,
display rendering, light sleep and pin reads aren't covered by these tests and
have to be checked on device.

CI (`.github/workflows/firmware.yml`) runs these tests and then builds both
boards on every push and pull request, so the board you are not holding cannot
break unnoticed.

## First-time setup

1. Flash the firmware and power on the device.
2. With no saved networks it opens a WPA2 access point named `BusAuntySetup`.
   The password is eight hex digits shown on the screen (derived from the
   chip's MAC, so it is different on every device).
3. Join that AP from your phone or laptop. A captive-portal page should open
   automatically (or browse to `192.168.4.1`).
4. Add your first WiFi network — pick it from the scan list if you can, so
   names with curly apostrophes (iPhone hotspots) do not have to be typed.
5. Fill in your bus stops on the same page, then save.
6. The device leaves the AP, joins the network you added, syncs its clock
   over NTP, and starts polling.

You can save up to five networks. The order of the list is the priority:
home first, office second, phone hotspot last, so the device will not sit
on cellular data when a real network is in range. It scans, tries the
highest-priority network it can see, and if nothing works it waits 5s, then
15s, then 60s between retries. A button press retries immediately — that is
how you catch a hotspot you just switched on.

A device that already had WiFi saved by the old firmware keeps that network
as #1 on first boot, so flashing this build does not send you back through
setup.

## Configuring the device

After it is on your LAN, hold the config button — KEY2 on the StickS3, D1
(the middle one) on the Feather — for 3 seconds. That unlocks
`http://busaunty.local` for five minutes (the IP is also shown on screen).
Outside that window the page asks you to press the button; saved passwords
are never sent to the browser. Press the same button again from that screen
to raise the setup AP if you cannot reach the device on the LAN.

The same page holds WiFi networks, four bus-stop slots, and the always-on
checkbox:

- **Code** (required): 3-5 digits, leading zeros preserved (`00481` stays
  `00481`). Rows with a missing or invalid code are dropped.
- **Name** (optional, up to 16 characters): shown in the header instead of the
  code. A name with no code is ignored.

With no stops configured, the screen prompts you to hold the config button
and open `busaunty.local`.

## Phone hotspot

The ESP32-S3 radio is 2.4 GHz only. Recent iPhones default Personal Hotspot
to 5 GHz, and the device will never see it. Turn on **Maximize Compatibility**
in the hotspot settings to force 2.4 GHz.

iOS also sleeps the hotspot radio unless the Personal Hotspot screen is open
or a client is already attached. Switch the hotspot on, then press a button
on the device so it scans immediately rather than waiting out a backoff.

## What's on screen

The arrivals screen shows a header of `<stop name or code> (<n>/<total>)`, a
battery icon in the top-right, and one row per service: service number on the
left, then its three arrival times right-aligned in three columns. Times are
rendered as minutes from now — `Due` for anything at or before now, `Nm` up to
an hour, `60+` beyond that, and `--` when the API gave no time for that slot.

Everything else is a full-screen status message: connecting to WiFi, syncing
time, loading a stop, no WiFi (press to retry), going to sleep and waking up,
or an error (`No data for X` for an HTTP 404, `Fetch failed (<status>)`
otherwise, `Bad response for X` for unparseable JSON, `X: no services` when the
stop has none).

## Sleep mode

On battery, the device does not stay lit and polling while nobody is looking at
it. Two idle stages, both counted from the last button press:

| Idle for | What happens |
| --- | --- |
| 30s | The backlight dims. The screen stays live and polling carries on |
| 2 min | `Sleeping...`, then the screen and the WiFi radio go off |

Either board can be sent to sleep immediately rather than waiting out the
timeout: press the Feather's middle button, or hold the StickS3's KEY1 for 1.5
seconds. Pressing any button wakes the device: it restores the screen,
reconnects, and fetches the current stop straight away rather than showing the
arrival times it had before it went down, which by then are minutes stale. The
press that wakes it does nothing else — it will not also page or change stop.

The device sleeps by light-sleeping the SoC with every button armed as a wake
source, rather than deep-sleeping it. RAM survives, so the configured stops
stay loaded, and the RTC keeps counting, so the NTP-synced clock is still right
on the other side; waking costs a WiFi reconnect (a second or two) instead of a
full boot and re-sync. Deep sleep would save roughly another 200µA, which on
this battery is not worth paying for on every glance at the screen.

Nothing sleeps while the device is externally powered — there is no battery to
conserve, and a display wired to USB is meant to stay on. How well a board can
tell differs, and neither can tell perfectly:

- **StickS3:** externally powered means the PMIC reports charging, or reports
  no battery attached. A device on USB with a *fully charged* battery reports
  neither, so it will still doze off.
- **Feather:** only charging counts. Its fuel gauge reads the charger rail when
  no battery is attached and reports a healthy charge, so a high percentage
  says nothing about the supply — an absent battery and a full one are
  indistinguishable.

For anything permanently plugged in, tick **"Always on (skip dimming and
sleep)"** on the config page. That is a deployment decision rather than
something the hardware can sense, so it is a saved setting: it disables both
idle stages outright, on either board.

The timings and the hold-to-sleep threshold are compile-time constants
(`kDimAfterMs`, `kSleepAfterMs`, `kSleepHoldMs` in `src/main.cpp`); the
brightness levels are per board, in `src/hal/<board>/board.cpp`. The decision
itself — awake, dimmed or asleep — is pure logic in
`src/core/sleep_policy.cpp` and is covered by the native unit tests, as are the
click/hold gestures in `src/core/button_gesture.cpp`. The light-sleep and
wake-source handling in `src/hal/<board>/sleep.cpp` is hardware-facing and has
to be checked on device.

## Battery indicator

Both boards sample every 2 seconds between fetches and smooth the result before
drawing it. The icon turns green while charging and red at or below 20%, and is
omitted entirely when the board has no reading to show.

- **StickS3:** the percentage comes from the PMIC's voltage reading, which sags
  while WiFi transmits — hence the smoothing. With no battery attached it reads
  negative and no icon is drawn.
- **Feather:** the percentage comes from a MAX17048 fuel gauge over I2C, which
  is steadier, but is smoothed identically so the icon behaves the same on both
  boards. If the gauge does not answer, no icon is drawn.

## Adding a board

Board-specific code lives in exactly four places, so a third board means:

1. A `BoardProfile` in `src/hal/<board>/board.cpp` — screen geometry, rotation,
   brightness levels, on-screen button names, and whether the board has a sleep
   button or can detect an absent battery.
2. Four implementations under `src/hal/<board>/`: `display_device.cpp`,
   `buttons.cpp`, `power.cpp` and `sleep.cpp`, against the headers in
   `include/hal/`.
3. One env in `firmware/platformio.ini` that `extends = esp32_base`, sets the
   board's `lib_deps`, and adds `build_src_filter` entries excluding every
   *other* board's directory.
4. A `-DBOARD_<NAME>` build flag, and a matching branch in
   `include/hal/gfx_types.h` aliasing that board's GFX types.

Add the env to `default_envs` so CI builds it. `include/hal/gfx_types.h` is the
only file in the tree with a board `#ifdef`; everything else is selected by
directory. Nothing board-specific belongs in `src/core/`, `src/net/`,
`src/storage/` or `src/ui/` — if a value differs per board, it goes on
`BoardProfile`, and if it is arithmetic, it goes in `src/core/` with tests.

## Notes

- The TLS root certificate pinned in `include/net/certs.h` (Google Trust
  Services "GTS Root R4") expires 2028-01-28. If `api.busaunty.com`'s
  certificate chain changes before then, HTTPS requests will start failing with
  a TLS handshake error until `certs.h` is updated with the new chain's root CA.
- Arrival times are computed as "minutes from now" against the device's
  NTP-synced UTC clock (`pool.ntp.org`, `time.nist.gov`) — see
  `src/core/eta_format.cpp`. The API's `EstimatedArrival` timestamps carry an
  explicit UTC offset, which `src/core/iso8601.cpp` normalizes to epoch
  seconds.
- The implementation plans, including per-task manual on-device test steps, are
  in `docs/superpowers/plans/2026-09-02-bus-arrival-display.md` (the original
  firmware) and `docs/superpowers/plans/2026-09-09-multi-device-support.md`
  (the board layer and the Feather).
