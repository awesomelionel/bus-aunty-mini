// firmware/src/hal/feather_s3_revtft/display_device.cpp
#include "hal/display_device.h"

#include <Arduino.h>

#include "board/board.h"
#include "rails.h"

namespace {

// The panel is a 1.14" ST7789 wired to SPI2. Pin numbers are the variant's
// own TFT_* defines, spelled out here because LovyanGFX wants plain ints.
constexpr int kPinSclk = 36;   // SCK
constexpr int kPinMosi = 35;   // MOSI
constexpr int kPinMiso = -1;   // the panel is write-only
constexpr int kPinDc = 40;     // TFT_DC
constexpr int kPinCs = 42;     // TFT_CS
constexpr int kPinRst = 41;    // TFT_RST
constexpr int kPinBacklight = 45;  // TFT_BACKLITE

// 240x135 landscape comes from a 135x240 panel window offset into the
// controller's 240x320 address space.
constexpr int kPanelWidth = 135;
constexpr int kPanelHeight = 240;
constexpr int kPanelOffsetX = 52;
constexpr int kPanelOffsetY = 40;

class FeatherDisplay : public lgfx::LGFX_Device {
 public:
    FeatherDisplay() {
        {
            auto cfg = bus_.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = kPinSclk;
            cfg.pin_mosi = kPinMosi;
            cfg.pin_miso = kPinMiso;
            cfg.pin_dc = kPinDc;
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
            cfg.offset_x = kPanelOffsetX;
            cfg.offset_y = kPanelOffsetY;
            cfg.offset_rotation = 0;
            cfg.invert = true;
            cfg.rgb_order = false;
            panel_.config(cfg);
        }
        {
            // PWM on the backlight, so setBrightness() works the same as it
            // does on the StickS3 rather than being on/off.
            auto cfg = light_.config();
            cfg.pin_bl = kPinBacklight;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            light_.config(cfg);
            panel_.setLight(&light_);
        }
        setPanel(&panel_);
    }

 private:
    lgfx::Panel_ST7789 panel_;
    lgfx::Bus_SPI bus_;
    lgfx::Light_PWM light_;
};

}  // namespace

namespace hal {

Gfx& gfx() {
    // Function-local so it is constructed on first use rather than at
    // static-init time, whatever order the HAL's translation units come up in.
    static FeatherDisplay instance;
    return instance;
}

void displayDeviceBegin() {
    feather::ensurePeripheralPowerOn();

    gfx().init();
    gfx().setRotation(board().rotation);
    gfx().setBrightness(board().brightnessFull);
}

void displayDeviceSetBrightness(uint8_t level) { gfx().setBrightness(level); }

void displayDeviceSleep() { gfx().sleep(); }

void displayDeviceWake() {
    gfx().wakeup();
    // Mirrors the StickS3: waking always comes back at full brightness, even
    // when the device dozed off from the dimmed state.
    gfx().setBrightness(board().brightnessFull);
}

}  // namespace hal
