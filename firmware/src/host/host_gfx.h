#pragma once
#include <stddef.h>
#include <stdint.h>

// Host stand-in for the LovyanGFX names ui/display.cpp draws with. Colour
// values match LovyanGFX: int/uint16 are RGB565, uint32 is RGB888.

static constexpr int TFT_BLACK = 0x0000;
static constexpr int TFT_NAVY = 0x000F;
static constexpr int TFT_DARKGREEN = 0x03E0;
static constexpr int TFT_DARKCYAN = 0x03EF;
static constexpr int TFT_MAROON = 0x7800;
static constexpr int TFT_PURPLE = 0x780F;
static constexpr int TFT_OLIVE = 0x7BE0;
static constexpr int TFT_LIGHTGREY = 0xD69A;
static constexpr int TFT_DARKGREY = 0x7BEF;
static constexpr int TFT_BLUE = 0x001F;
static constexpr int TFT_GREEN = 0x07E0;
static constexpr int TFT_CYAN = 0x07FF;
static constexpr int TFT_RED = 0xF800;
static constexpr int TFT_MAGENTA = 0xF81F;
static constexpr int TFT_YELLOW = 0xFFE0;
static constexpr int TFT_WHITE = 0xFFFF;
static constexpr int TFT_ORANGE = 0xFDA0;
static constexpr int TFT_SILVER = 0xC618;

enum textdatum_t : uint8_t {
    top_left = 0,
    top_center = 1,
    top_right = 2,
    middle_left = 4,
    middle_center = 5,
    middle_right = 6,
    bottom_left = 8,
    bottom_center = 9,
    bottom_right = 10,
};

namespace lgfx {
inline namespace v1 {

struct GFXglyph {
    uint32_t bitmapOffset;
    uint8_t width, height, xAdvance;
    int8_t xOffset, yOffset;
};

struct GFXfont {
    uint8_t* bitmap;
    GFXglyph* glyph;
    uint16_t first, last;
    uint8_t yAdvance;
};

namespace fonts {
extern const GFXfont DejaVu9;
extern const GFXfont DejaVu12;
extern const GFXfont DejaVu18;
extern const GFXfont DejaVu24;
}  // namespace fonts
}  // namespace v1
}  // namespace lgfx

namespace fonts {
using namespace lgfx::v1::fonts;
}

inline uint32_t hostRgb565To888(uint16_t c) {
    const uint8_t r5 = (c >> 11) & 0x1F;
    const uint8_t g6 = (c >> 5) & 0x3F;
    const uint8_t b5 = c & 0x1F;
    const uint8_t r = (r5 << 3) | (r5 >> 2);
    const uint8_t g = (g6 << 2) | (g6 >> 4);
    const uint8_t b = (b5 << 3) | (b5 >> 2);
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

// LovyanGFX treats uint32 as RGB888 and every narrower integer as RGB565.
template <typename T>
inline uint32_t hostToRgb888(T color) {
    return hostRgb565To888(static_cast<uint16_t>(color));
}
template <>
inline uint32_t hostToRgb888<uint32_t>(uint32_t color) {
    return color & 0xFFFFFF;
}

namespace hal {

class HostGfx {};
class HostCanvas;

class HostCanvas {
 public:
    explicit HostCanvas(HostGfx*);
    explicit HostCanvas(HostCanvas*);

    void setColorDepth(int depth);
    bool createSprite(int w, int h);
    void deleteSprite();
    void setFont(const lgfx::GFXfont* font);
    void setTextFont(int font);
    void setTextSize(int size);
    void setTextDatum(uint8_t datum);
    void setTextDatum(textdatum_t datum) { setTextDatum(static_cast<uint8_t>(datum)); }
    int textWidth(const char* text);
    int fontHeight() const;
    int drawString(const char* text, int x, int y);
    void pushSprite(int x, int y);
    void drawBitmap(int x, int y, const uint8_t* bitmap, int w, int h, int color);
    void setPivot(float x, float y);

    template <typename T>
    void setTextColor(T color) {
        const uint32_t rgb = hostToRgb888(color);
        fg_ = rgb;
        bg_ = rgb;
    }
    template <typename T1, typename T2>
    void setTextColor(T1 fg, T2 bg) {
        fg_ = hostToRgb888(fg);
        bg_ = hostToRgb888(bg);
    }
    template <typename T>
    void fillSprite(T color) {
        fillRect(0, 0, width_, height_, color);
    }
    template <typename T>
    void fillRect(int x, int y, int w, int h, T color) {
        fillRgb(x, y, w, h, hostToRgb888(color));
    }
    template <typename T>
    void drawRect(int x, int y, int w, int h, T color) {
        drawRectRgb(x, y, w, h, hostToRgb888(color));
    }
    template <typename T>
    void drawFastHLine(int x, int y, int w, T color) {
        fillRgb(x, y, w, 1, hostToRgb888(color));
    }
    template <typename T>
    void drawFastVLine(int x, int y, int h, T color) {
        fillRgb(x, y, 1, h, hostToRgb888(color));
    }
    template <typename T>
    void fillCircle(int x, int y, int r, T color) {
        fillCircleRgb(x, y, r, hostToRgb888(color));
    }
    template <typename T>
    void drawCircle(int x, int y, int r, T color) {
        drawCircleRgb(x, y, r, hostToRgb888(color));
    }
    template <typename T>
    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, T color) {
        fillTriangleRgb(x0, y0, x1, y1, x2, y2, hostToRgb888(color));
    }
    template <typename T>
    void pushRotateZoomWithAA(HostCanvas*, float, float, float, float, float, T) {}

    int width() const { return width_; }
    int height() const { return height_; }
    const uint32_t* pixels() const { return pixels_; }

 private:
    void fillRgb(int x, int y, int w, int h, uint32_t rgb);
    void drawRectRgb(int x, int y, int w, int h, uint32_t rgb);
    void fillCircleRgb(int x, int y, int r, uint32_t rgb);
    void drawCircleRgb(int x, int y, int r, uint32_t rgb);
    void fillTriangleRgb(int x0, int y0, int x1, int y1, int x2, int y2,
                         uint32_t rgb);
    uint32_t quantize(uint32_t rgb) const;
    void put(int x, int y, uint32_t rgb);

    int width_ = 0;
    int height_ = 0;
    int depth_ = 16;
    uint32_t* pixels_ = nullptr;
    const lgfx::GFXfont* font_ = nullptr;
    int textFont_ = 2;
    uint8_t datum_ = top_left;
    uint32_t fg_ = 0xFFFFFF;
    uint32_t bg_ = 0;
    int ascent_ = 0;
    int glyphHeight_ = 8;
    int yOffset_ = 0;
};

using Gfx = HostGfx;
using Canvas = HostCanvas;

Gfx& gfx();

}  // namespace hal

void hostSetBoard(const char* name);
bool hostSavePng(const char* path);
int hostFontHeight(const lgfx::GFXfont* font);
int hostTextWidth(const lgfx::GFXfont* font, const char* text);

// The sprite ui/display.cpp draws into. Zero before displaySetup().
int hostSpriteWidth();
int hostSpriteHeight();
uint32_t hostPixel(int x, int y);
