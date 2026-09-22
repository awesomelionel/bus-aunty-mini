// firmware/src/hal/tdisplay_s3/display_device.cpp
#include "hal/display_device.h"

#include <Arduino.h>

#include "board/board.h"
#include "rails.h"

namespace {

// Unlike the other two boards, this panel is not on SPI: it is an ST7789
// driven over an 8-bit Intel 8080 parallel bus, which on the ESP32-S3 is the
// LCD_CAM peripheral. That is why the bus below is Bus_Parallel8 and why
// eight data pins have to be named instead of one MOSI.
constexpr int kPinD0 = 39;
constexpr int kPinD1 = 40;
constexpr int kPinD2 = 41;
constexpr int kPinD3 = 42;
constexpr int kPinD4 = 45;
constexpr int kPinD5 = 46;
constexpr int kPinD6 = 47;
constexpr int kPinD7 = 48;
constexpr int kPinWr = 8;
constexpr int kPinRd = 9;
constexpr int kPinDc = 7;   // LCD_DC, the bus's "rs" line
constexpr int kPinCs = 6;
constexpr int kPinRst = 5;
constexpr int kPinBacklight = 38;

// 320x170 landscape comes from a 170x320 panel, and the ST7789 addresses it
// inside a 240x320 memory window. The visible columns are centred in that
// window, so they start 35 in ((240 - 170) / 2).
constexpr int kPanelWidth = 170;
constexpr int kPanelHeight = 320;
constexpr int kMemoryWidth = 240;
constexpr int kMemoryHeight = 320;
constexpr int kPanelOffsetX = 35;
constexpr int kPanelOffsetY = 0;

// The vendor's examples clock the bus here and warn that both slower and
// faster settings can show up as mosaic on the panel, so this starts where
// they are rather than at LovyanGFX's ceiling.
constexpr uint32_t kBusFreqWrite = 16000000;

class TDisplayS3Display : public lgfx::LGFX_Device {
 public:
    TDisplayS3Display() {
        {
            auto cfg = bus_.config();
            cfg.port = 0;  // the S3 has exactly one LCD_CAM
            cfg.freq_write = kBusFreqWrite;
            cfg.pin_wr = kPinWr;
            cfg.pin_rd = kPinRd;
            cfg.pin_rs = kPinDc;
            cfg.pin_d0 = kPinD0;
            cfg.pin_d1 = kPinD1;
            cfg.pin_d2 = kPinD2;
            cfg.pin_d3 = kPinD3;
            cfg.pin_d4 = kPinD4;
            cfg.pin_d5 = kPinD5;
            cfg.pin_d6 = kPinD6;
            cfg.pin_d7 = kPinD7;
            bus_.config(cfg);
            panel_.setBus(&bus_);
        }
        {
            auto cfg = panel_.config();
            cfg.pin_cs = kPinCs;
            cfg.pin_rst = kPinRst;
            cfg.pin_busy = -1;
            cfg.panel_width = kPanelWidth;
            cfg.panel_height = kPanelHeight;
            cfg.memory_width = kMemoryWidth;
            cfg.memory_height = kMemoryHeight;
            cfg.offset_x = kPanelOffsetX;
            cfg.offset_y = kPanelOffsetY;
            cfg.offset_rotation = 0;
            // This is an IPS panel, so its idle state is inverted relative to
            // the controller's default. If the first flash comes up in
            // photographic negative, this is the line.
            cfg.invert = true;
            cfg.rgb_order = false;
            panel_.config(cfg);
        }
        {
            // The vendor's examples only ever switch the backlight on and
            // off. PWM here instead, so setBrightness() and the dim-on-idle
            // stage behave the same as they do on the other two boards.
            auto cfg = light_.config();
            cfg.pin_bl = kPinBacklight;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            cfg.invert = false;  // backlight is active high on this board
            light_.config(cfg);
            panel_.setLight(&light_);
        }
        setPanel(&panel_);
    }

 private:
    lgfx::Panel_ST7789 panel_;
    lgfx::Bus_Parallel8 bus_;
    lgfx::Light_PWM light_;
};

}  // namespace

namespace hal {

Gfx& gfx() {
    // Function-local so it is constructed on first use rather than at
    // static-init time, whatever order the HAL's translation units come up in.
    static TDisplayS3Display instance;
    return instance;
}

void displayDeviceBegin() {
    tdisplay::ensurePeripheralPowerOn();

    gfx().init();
    gfx().setRotation(board().rotation);
    gfx().setBrightness(board().brightnessFull);
}

void displayDeviceSetBrightness(uint8_t level) { gfx().setBrightness(level); }

void displayDeviceSleep() { gfx().sleep(); }

void displayDeviceWake() {
    gfx().wakeup();
    // Mirrors the other two boards: waking always comes back at full
    // brightness, even when the device dozed off from the dimmed state.
    gfx().setBrightness(board().brightnessFull);
}

}  // namespace hal
