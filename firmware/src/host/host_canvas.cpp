#include "host/host_gfx.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

hal::HostCanvas* gRoot = nullptr;

struct Metrics {
    int ascent = 0;
    int height = 8;
    int yOffset = 0;
};

Metrics measureFont(const lgfx::GFXfont* font) {
    Metrics m;
    if (font == nullptr) {
        return m;
    }
    const int count = static_cast<int>(font->last) - static_cast<int>(font->first);
    int above = 0;
    int below = 0;
    for (int i = 0; i < count; ++i) {
        const lgfx::GFXglyph& glyph = font->glyph[i];
        const int a = -static_cast<int>(glyph.yOffset);
        if (a > above) {
            above = a;
        }
        const int b = static_cast<int>(glyph.height) - a;
        if (b > below) {
            below = b;
        }
    }
    m.ascent = above;
    m.height = above + below;
    m.yOffset = -above;
    return m;
}

const lgfx::GFXglyph* glyphFor(const lgfx::GFXfont* font, unsigned code) {
    if (font == nullptr || code < font->first || code > font->last) {
        return nullptr;
    }
    return &font->glyph[code - font->first];
}

int gfxTextWidth(const lgfx::GFXfont* font, const char* text) {
    if (font == nullptr || text == nullptr || text[0] == '\0') {
        return 0;
    }
    int left = 0;
    int right = 0;
    bool any = false;
    for (const char* p = text; *p != '\0'; ++p) {
        const lgfx::GFXglyph* glyph = glyphFor(font, static_cast<unsigned char>(*p));
        int xOffset = 0;
        int width = 0;
        int advance = font->yAdvance / 2;
        if (glyph != nullptr) {
            xOffset = glyph->xOffset;
            width = glyph->width;
            advance = glyph->xAdvance;
        }
        if (!any && xOffset < 0) {
            left = right = -xOffset;
        }
        any = true;
        right = left + std::max(advance, width + xOffset);
        left += advance;
    }
    return right;
}

void drawGlyph(hal::HostCanvas* canvas, const lgfx::GFXfont* font, int x, int y,
               unsigned code, uint32_t color, int yMetricOffset) {
    const lgfx::GFXglyph* glyph = glyphFor(font, code);
    if (glyph == nullptr || glyph->height == 0 || glyph->width == 0) {
        return;
    }
    y += yMetricOffset;
    x += glyph->xOffset;
    const int yBase = (-yMetricOffset) + glyph->yOffset;
    const uint8_t* bitmap = font->bitmap + glyph->bitmapOffset;
    const int w = glyph->width;
    const int h = glyph->height;
    uint8_t mask = 0x80;
    int32_t btmp = bitmap[0];
    if (btmp & mask) {
        btmp = ~btmp;
    }
    uint32_t bitlen = 0;
    int i = 0;
    int rowY = yBase;
    do {
        const int y0 = rowY;
        rowY = (++i) + yBase;
        uint32_t j = 0;
        uint32_t x0 = 0;
        uint32_t remain = static_cast<uint32_t>(w);
        do {
            if (bitlen == 0) {
                btmp = ~btmp;
                do {
                    do {
                        ++bitlen;
                        if ((mask >>= 1) == 0) {
                            goto next_byte;
                        }
                    } while (btmp & mask);
                    break;
                next_byte:
                    mask = 0x80;
                    btmp = static_cast<int32_t>(*++bitmap) ^ (btmp < 0 ? ~0 : 0);
                } while (btmp & mask);
            }
            const uint32_t run = std::min(bitlen, remain);
            remain -= run;
            bitlen -= run;
            j += run;
            if (btmp >= 0 && j > x0) {
                canvas->fillRect(x + static_cast<int>(x0), y + y0,
                                 static_cast<int>(j - x0), 1, color);
            }
            x0 = j;
        } while (remain > 0);
    } while (i < h);
}

// Font 2 (the 16px built-in) as a 5x7 scaled by 2, so status screens stay
// readable without pulling the proportional GLCD table into the host.
const uint8_t kFont5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00, 0x07, 0x00, 0x07, 0x00,
    0x14, 0x7F, 0x14, 0x7F, 0x14, 0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x23, 0x13, 0x08, 0x64, 0x62,
    0x36, 0x49, 0x55, 0x22, 0x50, 0x00, 0x05, 0x03, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x41, 0x00,
    0x00, 0x41, 0x22, 0x1C, 0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, 0x08, 0x08, 0x3E, 0x08, 0x08,
    0x00, 0x50, 0x30, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x60, 0x60, 0x00, 0x00,
    0x20, 0x10, 0x08, 0x04, 0x02, 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x42, 0x7F, 0x40, 0x00,
    0x42, 0x61, 0x51, 0x49, 0x46, 0x21, 0x41, 0x45, 0x4B, 0x31, 0x18, 0x14, 0x12, 0x7F, 0x10,
    0x27, 0x45, 0x45, 0x45, 0x39, 0x3C, 0x4A, 0x49, 0x49, 0x30, 0x01, 0x71, 0x09, 0x05, 0x03,
    0x36, 0x49, 0x49, 0x49, 0x36, 0x06, 0x49, 0x49, 0x29, 0x1E, 0x00, 0x36, 0x36, 0x00, 0x00,
    0x00, 0x56, 0x36, 0x00, 0x00, 0x08, 0x14, 0x22, 0x41, 0x00, 0x14, 0x14, 0x14, 0x14, 0x14,
    0x00, 0x41, 0x22, 0x14, 0x08, 0x02, 0x01, 0x51, 0x09, 0x06, 0x32, 0x49, 0x79, 0x41, 0x3E,
    0x7E, 0x11, 0x11, 0x11, 0x7E, 0x7F, 0x49, 0x49, 0x49, 0x36, 0x3E, 0x41, 0x41, 0x41, 0x22,
    0x7F, 0x41, 0x41, 0x22, 0x1C, 0x7F, 0x49, 0x49, 0x49, 0x41, 0x7F, 0x09, 0x09, 0x09, 0x01,
    0x3E, 0x41, 0x49, 0x49, 0x7A, 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x41, 0x7F, 0x41, 0x00,
    0x20, 0x40, 0x41, 0x3F, 0x01, 0x7F, 0x08, 0x14, 0x22, 0x41, 0x7F, 0x40, 0x40, 0x40, 0x40,
    0x7F, 0x02, 0x0C, 0x02, 0x7F, 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x3E, 0x41, 0x41, 0x41, 0x3E,
    0x7F, 0x09, 0x09, 0x09, 0x06, 0x3E, 0x41, 0x51, 0x21, 0x5E, 0x7F, 0x09, 0x19, 0x29, 0x46,
    0x46, 0x49, 0x49, 0x49, 0x31, 0x01, 0x01, 0x7F, 0x01, 0x01, 0x3F, 0x40, 0x40, 0x40, 0x3F,
    0x1F, 0x20, 0x40, 0x20, 0x1F, 0x3F, 0x40, 0x38, 0x40, 0x3F, 0x63, 0x14, 0x08, 0x14, 0x63,
    0x07, 0x08, 0x70, 0x08, 0x07, 0x61, 0x51, 0x49, 0x45, 0x43, 0x00, 0x7F, 0x41, 0x41, 0x00,
    0x02, 0x04, 0x08, 0x10, 0x20, 0x00, 0x41, 0x41, 0x7F, 0x00, 0x04, 0x02, 0x01, 0x02, 0x04,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x01, 0x02, 0x04, 0x00, 0x20, 0x54, 0x54, 0x54, 0x78,
    0x7F, 0x48, 0x44, 0x44, 0x38, 0x38, 0x44, 0x44, 0x44, 0x20, 0x38, 0x44, 0x44, 0x48, 0x7F,
    0x38, 0x54, 0x54, 0x54, 0x18, 0x08, 0x7E, 0x09, 0x01, 0x02, 0x0C, 0x52, 0x52, 0x52, 0x3E,
    0x7F, 0x08, 0x04, 0x04, 0x78, 0x00, 0x44, 0x7D, 0x40, 0x00, 0x20, 0x40, 0x44, 0x3D, 0x00,
    0x7F, 0x10, 0x28, 0x44, 0x00, 0x00, 0x41, 0x7F, 0x40, 0x00, 0x7C, 0x04, 0x18, 0x04, 0x78,
    0x7C, 0x08, 0x04, 0x04, 0x78, 0x38, 0x44, 0x44, 0x44, 0x38, 0x7C, 0x14, 0x14, 0x14, 0x08,
    0x08, 0x14, 0x14, 0x18, 0x7C, 0x7C, 0x08, 0x04, 0x04, 0x08, 0x48, 0x54, 0x54, 0x54, 0x20,
    0x04, 0x3F, 0x44, 0x40, 0x20, 0x3C, 0x40, 0x40, 0x20, 0x7C, 0x1C, 0x20, 0x40, 0x20, 0x1C,
    0x3C, 0x40, 0x30, 0x40, 0x3C, 0x44, 0x28, 0x10, 0x28, 0x44, 0x0C, 0x50, 0x50, 0x50, 0x3C,
    0x44, 0x64, 0x54, 0x4C, 0x44, 0x00, 0x08, 0x36, 0x41, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00,
    0x00, 0x41, 0x36, 0x08, 0x00, 0x08, 0x08, 0x2A, 0x1C, 0x08,
};

int builtinWidth(const char* text) {
    if (text == nullptr) {
        return 0;
    }
    int n = 0;
    for (const char* p = text; *p != '\0'; ++p) {
        ++n;
    }
    return n * 12;
}

void drawBuiltin(hal::HostCanvas* canvas, const char* text, int x, int y, uint32_t color) {
    for (const char* p = text; *p != '\0'; ++p) {
        unsigned char ch = static_cast<unsigned char>(*p);
        if (ch < 32 || ch > 126) {
            ch = '?';
        }
        const uint8_t* col = kFont5x7 + (ch - 32) * 5;
        for (int cx = 0; cx < 5; ++cx) {
            for (int cy = 0; cy < 7; ++cy) {
                if (col[cx] & (1 << cy)) {
                    canvas->fillRect(x + cx * 2, y + cy * 2, 2, 2, color);
                }
            }
        }
        x += 12;
    }
}

uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            const uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

void putBe32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v >> 24));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v));
}

void pngChunk(std::vector<uint8_t>& out, const char* type, const uint8_t* data, size_t len) {
    putBe32(out, static_cast<uint32_t>(len));
    const size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data, data + len);
    const uint32_t crc = crc32(out.data() + start, 4 + len);
    putBe32(out, crc);
}

uint32_t adler32(const uint8_t* data, size_t len) {
    uint32_t a = 1;
    uint32_t b = 0;
    for (size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

}  // namespace

namespace hal {

HostCanvas::HostCanvas(HostGfx*) { gRoot = this; }
HostCanvas::HostCanvas(HostCanvas*) {}

void HostCanvas::setColorDepth(int depth) { depth_ = depth; }

bool HostCanvas::createSprite(int w, int h) {
    delete[] pixels_;
    pixels_ = nullptr;
    width_ = 0;
    height_ = 0;
    if (w <= 0 || h <= 0) {
        return false;
    }
    pixels_ = new uint32_t[static_cast<size_t>(w) * static_cast<size_t>(h)]();
    width_ = w;
    height_ = h;
    return true;
}

void HostCanvas::deleteSprite() {
    delete[] pixels_;
    pixels_ = nullptr;
    width_ = 0;
    height_ = 0;
}

void HostCanvas::setFont(const lgfx::GFXfont* font) {
    font_ = font;
    textFont_ = 0;
    const Metrics m = measureFont(font);
    ascent_ = m.ascent;
    glyphHeight_ = m.height;
    yOffset_ = m.yOffset;
}

void HostCanvas::setTextFont(int font) {
    font_ = nullptr;
    textFont_ = font;
    glyphHeight_ = 16;
    yOffset_ = 0;
    ascent_ = 14;
}

void HostCanvas::setTextSize(int) {}

void HostCanvas::setTextDatum(uint8_t datum) { datum_ = datum; }

int HostCanvas::textWidth(const char* text) {
    if (font_ != nullptr) {
        return gfxTextWidth(font_, text);
    }
    return builtinWidth(text);
}

int HostCanvas::fontHeight() const { return glyphHeight_; }

int HostCanvas::drawString(const char* text, int x, int y) {
    if (text == nullptr) {
        return 0;
    }
    const int width = textWidth(text);
    const int height = fontHeight();
    if (datum_ & top_center) {
        x -= width / 2;
    } else if (datum_ & top_right) {
        x -= width;
    }
    if (datum_ & middle_left) {
        y -= height / 2;
    }
    y -= yOffset_;
    if (font_ != nullptr) {
        int cursor = x;
        for (const char* p = text; *p != '\0'; ++p) {
            const unsigned code = static_cast<unsigned char>(*p);
            const lgfx::GFXglyph* glyph = glyphFor(font_, code);
            drawGlyph(this, font_, cursor, y, code, fg_, yOffset_);
            cursor += glyph != nullptr ? glyph->xAdvance : font_->yAdvance / 2;
        }
        return width;
    }
    drawBuiltin(this, text, x, y, fg_);
    return width;
}

void HostCanvas::pushSprite(int, int) {}

void HostCanvas::drawBitmap(int, int, const uint8_t*, int, int, int) {}

void HostCanvas::setPivot(float, float) {}

uint32_t HostCanvas::quantize(uint32_t rgb) const {
    if (depth_ != 8) {
        return rgb;
    }
    const uint8_t r = (rgb >> 16) & 0xFF;
    const uint8_t g = (rgb >> 8) & 0xFF;
    const uint8_t b = rgb & 0xFF;
    const uint8_t r3 = r >> 5;
    const uint8_t g3 = g >> 5;
    const uint8_t b2 = b >> 6;
    const uint8_t R = static_cast<uint8_t>((((r3 << 3) + r3) << 2) + (r3 >> 1));
    const uint8_t G = static_cast<uint8_t>((((g3 << 3) + g3) << 2) + (g3 >> 1));
    const uint8_t B = static_cast<uint8_t>(b2 * 0x55);
    return (static_cast<uint32_t>(R) << 16) | (static_cast<uint32_t>(G) << 8) | B;
}

void HostCanvas::put(int x, int y, uint32_t rgb) {
    if (pixels_ == nullptr || x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }
    pixels_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)] =
        quantize(rgb);
}

void HostCanvas::fillRgb(int x, int y, int w, int h, uint32_t rgb) {
    if (w < 0) {
        x += w;
        w = -w;
    }
    if (h < 0) {
        y += h;
        h = -h;
    }
    for (int yy = y; yy < y + h; ++yy) {
        for (int xx = x; xx < x + w; ++xx) {
            put(xx, yy, rgb);
        }
    }
}

void HostCanvas::drawRectRgb(int x, int y, int w, int h, uint32_t rgb) {
    fillRgb(x, y, w, 1, rgb);
    fillRgb(x, y + h - 1, w, 1, rgb);
    fillRgb(x, y, 1, h, rgb);
    fillRgb(x + w - 1, y, 1, h, rgb);
}

void HostCanvas::fillCircleRgb(int cx, int cy, int r, uint32_t rgb) {
    for (int y = -r; y <= r; ++y) {
        for (int x = -r; x <= r; ++x) {
            if (x * x + y * y <= r * r) {
                put(cx + x, cy + y, rgb);
            }
        }
    }
}

void HostCanvas::drawCircleRgb(int cx, int cy, int r, uint32_t rgb) {
    int x = r;
    int y = 0;
    int err = 1 - x;
    while (x >= y) {
        put(cx + x, cy + y, rgb);
        put(cx + y, cy + x, rgb);
        put(cx - y, cy + x, rgb);
        put(cx - x, cy + y, rgb);
        put(cx - x, cy - y, rgb);
        put(cx - y, cy - x, rgb);
        put(cx + y, cy - x, rgb);
        put(cx + x, cy - y, rgb);
        ++y;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            --x;
            err += 2 * (y - x) + 1;
        }
    }
}

void HostCanvas::fillTriangleRgb(int x0, int y0, int x1, int y1, int x2, int y2,
                                uint32_t rgb) {
    if (y0 > y1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    if (y1 > y2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }
    if (y0 > y1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    auto edge = [](int y, int yA, int xA, int yB, int xB) {
        if (yB == yA) {
            return xA;
        }
        return xA + (xB - xA) * (y - yA) / (yB - yA);
    };
    for (int y = y0; y <= y2; ++y) {
        const int xa = y < y1 ? edge(y, y0, x0, y1, x1) : edge(y, y1, x1, y2, x2);
        const int xb = edge(y, y0, x0, y2, x2);
        fillRgb(std::min(xa, xb), y, std::abs(xa - xb) + 1, 1, rgb);
    }
}

}  // namespace hal

int hostFontHeight(const lgfx::GFXfont* font) { return measureFont(font).height; }

int hostTextWidth(const lgfx::GFXfont* font, const char* text) {
    return gfxTextWidth(font, text);
}

int hostSpriteWidth() { return gRoot != nullptr ? gRoot->width() : 0; }

int hostSpriteHeight() { return gRoot != nullptr ? gRoot->height() : 0; }

uint32_t hostPixel(int x, int y) {
    if (gRoot == nullptr || gRoot->pixels() == nullptr || x < 0 || y < 0 ||
        x >= gRoot->width() || y >= gRoot->height()) {
        return 0;
    }
    return gRoot->pixels()[static_cast<size_t>(y) * static_cast<size_t>(gRoot->width()) +
                           static_cast<size_t>(x)];
}

bool hostSavePng(const char* path) {
    if (gRoot == nullptr || gRoot->pixels() == nullptr) {
        return false;
    }
    const int w = gRoot->width();
    const int h = gRoot->height();
    const uint32_t* px = gRoot->pixels();
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(h) * (1 + static_cast<size_t>(w) * 3));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        for (int x = 0; x < w; ++x) {
            const uint32_t c = px[static_cast<size_t>(y) * static_cast<size_t>(w) +
                                 static_cast<size_t>(x)];
            raw.push_back(static_cast<uint8_t>(c >> 16));
            raw.push_back(static_cast<uint8_t>(c >> 8));
            raw.push_back(static_cast<uint8_t>(c));
        }
    }
    std::vector<uint8_t> zlib;
    zlib.push_back(0x78);
    zlib.push_back(0x01);
    size_t offset = 0;
    while (offset < raw.size()) {
        const size_t n = std::min<size_t>(65535, raw.size() - offset);
        const bool last = offset + n == raw.size();
        zlib.push_back(last ? 1 : 0);
        zlib.push_back(static_cast<uint8_t>(n & 0xFF));
        zlib.push_back(static_cast<uint8_t>((n >> 8) & 0xFF));
        const uint16_t nnot = static_cast<uint16_t>(~static_cast<uint16_t>(n));
        zlib.push_back(static_cast<uint8_t>(nnot & 0xFF));
        zlib.push_back(static_cast<uint8_t>((nnot >> 8) & 0xFF));
        zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                    raw.begin() + static_cast<std::ptrdiff_t>(offset + n));
        offset += n;
    }
    const uint32_t ad = adler32(raw.data(), raw.size());
    zlib.push_back(static_cast<uint8_t>(ad >> 24));
    zlib.push_back(static_cast<uint8_t>(ad >> 16));
    zlib.push_back(static_cast<uint8_t>(ad >> 8));
    zlib.push_back(static_cast<uint8_t>(ad));

    uint8_t ihdr[13];
    ihdr[0] = static_cast<uint8_t>(w >> 24);
    ihdr[1] = static_cast<uint8_t>(w >> 16);
    ihdr[2] = static_cast<uint8_t>(w >> 8);
    ihdr[3] = static_cast<uint8_t>(w);
    ihdr[4] = static_cast<uint8_t>(h >> 24);
    ihdr[5] = static_cast<uint8_t>(h >> 16);
    ihdr[6] = static_cast<uint8_t>(h >> 8);
    ihdr[7] = static_cast<uint8_t>(h);
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;

    std::vector<uint8_t> png = {137, 80, 78, 71, 13, 10, 26, 10};
    pngChunk(png, "IHDR", ihdr, sizeof(ihdr));
    pngChunk(png, "IDAT", zlib.data(), zlib.size());
    pngChunk(png, "IEND", nullptr, 0);
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) {
        return false;
    }
    const size_t wrote = std::fwrite(png.data(), 1, png.size(), f);
    std::fclose(f);
    return wrote == png.size();
}
