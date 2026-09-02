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
button input) isn't unit-testable this way.

**⚠️ This firmware has never been run on physical hardware.** It compiles
cleanly and its pure logic (ISO-8601 parsing, ETA formatting, JSON parsing)
is unit-tested, but the following are all UNVERIFIED and must be checked
before trusting the device:
- Boot and screen orientation/rotation
- WiFi captive-portal walkthrough (first-time provisioning)
- Display layout — no clipping or overlap across all 6 rows and the status
  screens (especially multi-line status messages)
- Button-driven stop cycling
- Live 30-second poll cycle against the real API, with countdown ticking
- WiFi-loss detection and recovery
- HTTP error handling (e.g. an invalid bus stop code)

Work through the on-device checklist in
`../docs/superpowers/plans/2026-09-02-bus-arrival-display.md` (each task's
"Manual on-device test" steps) before relying on this firmware.

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
