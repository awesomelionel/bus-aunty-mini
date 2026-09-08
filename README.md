# Bus Aunty Display Firmware

ESP32-S3 (M5Stack StickS3) firmware that polls
`GET https://api.busaunty.com/api/v1/BusArrival?BusStopCode=<code>` every 30
seconds and shows up to 6 bus services, with all 3 arrival times each, for up
to 4 configured bus stops. Bus stops and WiFi credentials are both set on the
device through a captive portal — nothing is compiled in.

## Hardware

- [M5Stack StickS3](https://docs.m5stack.com/en/core/StickS3) (ESP32-S3-PICO-1-N8R8,
  8MB flash, 8MB Octal PSRAM, 135x240 ST7789P3 display driven in 240x135
  landscape via `setRotation(1)`).
- USB-C cable for flashing and power. The internal battery is optional; when
  none is attached the battery icon is simply not drawn.

Buttons:

| Button | Action |
| --- | --- |
| KEY1, front (`M5.BtnA`) | Press to cycle to the next configured stop and fetch it immediately |
| KEY1, front (`M5.BtnA`) | Hold 1.5s to sleep now |
| KEY2, side (`M5.BtnB`) | Hold 3s to reopen the setup portal and edit stops or WiFi |
| Either button | Press to wake from sleep |

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

The pure logic in `src/core/` (ISO-8601 parsing, ETA formatting, JSON parsing,
bus stop config validation, the sleep/dim decision) runs as host-native unit
tests — no device needed:

```bash
cd firmware
pio test -e native
```

The `native` environment excludes `main.cpp`, `src/net/`, `src/power/`,
`src/storage/` and `src/ui/`, so WiFi provisioning, HTTPS fetch, NVS storage,
display rendering, light sleep and button input aren't covered by these tests
and have to be checked on device.

## First-time setup

1. Flash the firmware and power on the device.
2. It opens an open WiFi access point named `BusAuntySetup`.
3. Connect to it from your phone or laptop; a captive-portal page should open
   automatically (or browse to `192.168.4.1`).
4. Choose "Configure WiFi", pick your network, and enter its password.
5. Open the "Setup" page and fill in your bus stops (see below), then save.
6. The device syncs its clock over NTP and starts polling.

The portal times out after 180 seconds. On a first boot with no saved
credentials, a timeout makes the device restart and try again. WiFi credentials
and bus stops are stored on-device (WiFiManager's own storage and the
`busaunty` NVS namespace respectively), never in source, and are reused on
every future boot.

## Configuring bus stops

Bus stops live on the portal's own "Setup" page — separate from the WiFi page
so editing a stop doesn't mean retyping your WiFi password. There are 4 slots,
each with:

- **Code** (required): 3-5 digits, leading zeros preserved (`00481` stays
  `00481`). Rows with a missing or invalid code are dropped.
- **Name** (optional, up to 16 characters): shown in the header instead of the
  code. A name with no code is ignored.

To change stops after setup, hold the side button (KEY2) for 3 seconds — the
portal reopens with your current stops pre-filled, and saving writes them back
to NVS only if they actually changed. With no stops configured, the screen
prompts you to do exactly this.

## What's on screen

The arrivals screen shows a header of `<stop name or code> (<n>/<total>)`, a
battery icon in the top-right, and one row per service: service number on the
left, then its three arrival times right-aligned in three columns. Times are
rendered as minutes from now — `Due` for anything at or before now, `Nm` up to
an hour, `60+` beyond that, and `--` when the API gave no time for that slot.

Everything else is a full-screen status message: connecting to WiFi, syncing
time, loading a stop, WiFi lost and reconnecting, going to sleep and waking up,
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

Holding KEY1 for 1.5 seconds sleeps immediately, without waiting out the
timeout. Pressing either button wakes the device: it restores the screen,
reconnects, and fetches the current stop straight away rather than showing the
arrival times it had before it went down, which by then are minutes stale. The
press that wakes it does nothing else — it will not also page or change stop.

The device sleeps by light-sleeping the SoC with both buttons armed as wake
sources, rather than deep-sleeping it. RAM survives, so the configured stops
stay loaded, and the RTC keeps counting, so the NTP-synced clock is still right
on the other side; waking costs a WiFi reconnect (a second or two) instead of a
full boot and re-sync. Deep sleep would save roughly another 200µA, which on
this battery is not worth paying for on every glance at the screen.

Nothing sleeps while the device is externally powered — there is no battery to
conserve, and a display wired to USB is meant to stay on. "Externally powered"
is read from the PMIC as charging, or as no battery being attached, so a device
sitting on USB with a *fully charged* battery may report neither and still doze
off. One press brings it back.

The timings, the dim level and the hold-to-sleep threshold are compile-time
constants: `kDimAfterMs`, `kSleepAfterMs` and `kSleepHoldMs` in
`src/main.cpp`, and `kBrightnessFull`/`kBrightnessDim` in `src/ui/display.cpp`.
The decision itself — awake, dimmed or asleep — is pure logic in
`src/core/sleep_policy.cpp` and is covered by the native unit tests; the
light-sleep and wake-source handling in `src/power/` is hardware-facing and has
to be checked on device.

## Battery indicator

The battery percentage comes from the PMIC's voltage reading, which sags while
WiFi transmits, so it's sampled every 2 seconds between fetches and smoothed
before being drawn. The icon turns green while charging and red at or below
20%.

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
- The implementation plan, including per-task manual on-device test steps, is
  in `docs/superpowers/plans/2026-09-02-bus-arrival-display.md`.
