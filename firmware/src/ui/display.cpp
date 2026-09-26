// firmware/src/ui/display.cpp
#include "ui/display.h"

#include <cstdio>
#include <vector>

#include "board/board.h"
#include "core/eta_format.h"
#include "core/header_format.h"
#include "core/layout.h"
#include "core/night_window.h"
#include "core/text_utils.h"
#include "hal/display_device.h"
#include "ui/wifi_image.h"

namespace {

// Bound to the panel here rather than in displaySetup() because M5Canvas
// takes its PSRAM preference from the parent at construction. The sprite
// itself is not allocated until createSprite() below, and neither this nor
// hal::gfx() touches the hardware, so binding at static-init is safe.
hal::Canvas canvas(&hal::gfx());

// Two layouts: single-line for stops with no labels, two-line for stops with labels.
// Computed once in displaySetup(), selected at render time based on current data.
ArrivalsLayout arrivalsLayoutSingleLine;
ArrivalsLayout arrivalsLayoutTwoLine;

int screenWidth() { return board().screenWidth; }
int screenHeight() { return board().screenHeight; }

// The arrivals rows are drawn at whichever size the board asks for: the
// 240x135 panels only have room for DejaVu18, while a 320x170 one can afford
// the next size up. Returning the concrete GFXfont rather than an IFont keeps
// this working against M5GFX and LovyanGFX alike.
const lgfx::GFXfont* arrivalsFont() {
    return board().arrivalsFontHeight >= 24 ? &fonts::DejaVu24
                                            : &fonts::DejaVu18;
}

// Label font: DejaVu9 for small boards, DejaVu12 for larger boards.
const lgfx::GFXfont* labelFont() {
    return board().arrivalsFontHeight >= 24 ? &fonts::DejaVu12
                                            : &fonts::DejaVu9;
}

// Wrapper for truncateText that handles font setup and visit marker reservation.
// When BUS_AUNTY_SHOW_VISIT_MARKER is enabled and reserveMarkerWidth is true,
// reserves space for " 2nd" marker.
std::string truncateLabel(const std::string& text, int maxWidth,
                          const lgfx::GFXfont* font, bool reserveMarkerWidth = false) {
    if (text.empty()) {
        return text;
    }
    
    canvas.setFont(font);
    
    // Reserve space for " 2nd" marker when flag is enabled and reserveMarkerWidth is true
#if BUS_AUNTY_SHOW_VISIT_MARKER
    if (reserveMarkerWidth) {
        int markerWidth = canvas.textWidth(" 2nd");
        maxWidth -= markerWidth;
        if (maxWidth < 10) {  // Sanity check
            return ".";
        }
    }
#endif
    
    // Use core truncation with canvas width callback
    return truncateText(text, maxWidth, [](const char* s) {
        return canvas.textWidth(s);
    });
}

// A solid triangle of `size` pixels across, centred on the point given. This
// is what stands in for an arrow glyph, which the DejaVu fonts do not carry.
void drawArrow(int centerX, int centerY, int size, ButtonArrow arrow) {
    const int half = size / 2;
    switch (arrow) {
        case ButtonArrow::Up:
            canvas.fillTriangle(centerX - half, centerY + half,
                                centerX + half, centerY + half, centerX,
                                centerY - half, TFT_WHITE);
            break;
        case ButtonArrow::Down:
            canvas.fillTriangle(centerX - half, centerY - half,
                                centerX + half, centerY - half, centerX,
                                centerY + half, TFT_WHITE);
            break;
        case ButtonArrow::Left:
            canvas.fillTriangle(centerX + half, centerY - half,
                                centerX + half, centerY + half,
                                centerX - half, centerY, TFT_WHITE);
            break;
        case ButtonArrow::Right:
            canvas.fillTriangle(centerX - half, centerY - half,
                                centerX - half, centerY + half,
                                centerX + half, centerY, TFT_WHITE);
            break;
        case ButtonArrow::None:
            break;
    }
}

// Space either side of an inline arrow.
constexpr int kArrowGap = 3;

// One line of a centred message. When `arrow` is not None the line is drawn as
// `text` + arrow + `tail`, which is how a button gets pointed at mid-sentence
// in a font that has no arrow glyph.
struct MessageLine {
    std::string text;
    ButtonArrow arrow = ButtonArrow::None;
    std::string tail;
};

int messageLineWidth(const MessageLine& line, int arrowSize) {
    int width = canvas.textWidth(line.text.c_str());
    if (line.arrow != ButtonArrow::None) {
        width += kArrowGap + arrowSize + kArrowGap +
                 canvas.textWidth(line.tail.c_str());
    }
    return width;
}

void drawMessageLine(const MessageLine& line, int y, int arrowSize) {
    int x = (screenWidth() - messageLineWidth(line, arrowSize)) / 2;
    canvas.setTextDatum(top_left);
    canvas.drawString(line.text.c_str(), x, y);
    if (line.arrow == ButtonArrow::None) {
        return;
    }
    x += canvas.textWidth(line.text.c_str()) + kArrowGap;
    drawArrow(x + arrowSize / 2, y + canvas.fontHeight() / 2, arrowSize,
              line.arrow);
    x += arrowSize + kArrowGap;
    canvas.drawString(line.tail.c_str(), x, y);
}

// Largest first. A sentence that fits the 320x170 panel comfortably can still
// overrun a 240x135 one, so the size is measured per message rather than fixed
// per board -- there is no single answer that suits both.
const lgfx::GFXfont* const kFitFonts[] = {&fonts::DejaVu24, &fonts::DejaVu18,
                                          &fonts::DejaVu12, &fonts::DejaVu9};
constexpr int kFitFontCount =
    static_cast<int>(sizeof(kFitFonts) / sizeof(kFitFonts[0]));

// Selects the largest font in which every line fits across and all of them fit
// down, leaving it set on the canvas. Falls back to the smallest rather than
// letting a long line run off the edge.
void selectFittingFont(const MessageLine* lines, int count) {
    for (int f = 0; f < kFitFontCount; ++f) {
        canvas.setFont(kFitFonts[f]);
        const int lineHeight = canvas.fontHeight();
        if (lineHeight * count > screenHeight()) {
            continue;
        }
        const int arrowSize = lineHeight * 2 / 3;
        bool fits = true;
        for (int i = 0; i < count; ++i) {
            if (messageLineWidth(lines[i], arrowSize) > screenWidth()) {
                fits = false;
                break;
            }
        }
        if (fits) {
            return;
        }
    }
    canvas.setFont(kFitFonts[kFitFontCount - 1]);
}

// Draws the lines stacked and centred, at the largest size that fits, then
// puts the shared default font back for whatever screen comes next.
void drawCenteredMessage(const MessageLine* lines, int count) {
    selectFittingFont(lines, count);
    const int lineHeight = canvas.fontHeight();
    const int arrowSize = lineHeight * 2 / 3;

    int y = (screenHeight() - lineHeight * count) / 2;
    for (int i = 0; i < count; ++i) {
        drawMessageLine(lines[i], y, arrowSize);
        y += lineHeight;
    }
}

// The hold-this-button line, phrased for whichever way the board identifies
// its config button: pointed at where that is possible, named where it is not.
MessageLine secondaryButtonLine(const char* tail) {
    MessageLine line;
    if (board().secondaryButtonArrow == ButtonArrow::None) {
        line.text = std::string("Hold ") + board().secondaryButtonLabel + " " +
                    tail;
        return line;
    }
    line.text = "Hold the ";
    line.arrow = board().secondaryButtonArrow;
    line.tail = std::string(" Button ") + tail;
    return line;
}

constexpr int kDefaultTextFont = 2;  // 16px; see displayShowWifiSetup

// Each arrival is tinted by how full that bus is. Colour carries the load and
// nothing else, so an arriving bus is left to read as "Arr" on its own.
uint16_t loadColor(BusLoad load) {
    switch (load) {
        case BusLoad::SeatsAvailable:
            return TFT_GREEN;
        case BusLoad::StandingAvailable:
            return TFT_ORANGE;
        case BusLoad::LimitedStanding:
            return TFT_RED;
        case BusLoad::Unknown:
            break;
    }
    return TFT_WHITE;
}

// ── Windows 95 treatment ─────────────────────────────────────────────────
//
// Colours are given as RGB888 and handed to LovyanGFX as uint32_t, which is
// how it tells them from the uint16_t RGB565 the TFT_* macros carry. The
// sprite is 8-bit, so what lands on the panel is the nearest RGB332 -- fine
// for a palette that was designed for 16 colours in the first place, though
// blue has only two bits and the navy shifts a little.
struct Palette {
    uint32_t chrome, face, ink, hi, lo;
    uint32_t capA, capB, capInk;
    uint32_t seats, stand, limit, dim;
};

// The chrome is the VGA 16 on a silver ground. The load colours are not: the
// period-correct olive and maroon sat at almost the same brightness as each
// other and as the black service number beside them, so on a white list they
// collapsed into one another at 19px. Standing and limited are pushed up in
// both saturation and lightness so the three differ by more than hue alone.
constexpr Palette kDayPalette = {
    /*chrome=*/0xC0C0C0, /*face=*/0xFFFFFF, /*ink=*/0x000000,
    /*hi=*/0xFFFFFF,     /*lo=*/0x808080,
    /*capA=*/0x000080,   /*capB=*/0x1084D0, /*capInk=*/0xFFFFFF,
    /*seats=*/0x008000,  /*stand=*/0xE07800, /*limit=*/0xD00000,
    /*dim=*/0x606060,
};

// Windows' own High Contrast Black, which is both period-correct and the
// right answer for a dark room: silver chrome at 3am is a lamp.
constexpr Palette kNightPalette = {
    /*chrome=*/0x000000, /*face=*/0x000000, /*ink=*/0xFFFFFF,
    /*hi=*/0xFFFFFF,     /*lo=*/0x5A5A5A,
    /*capA=*/0x000000,   /*capB=*/0x000000, /*capInk=*/0xFFFF00,
    /*seats=*/0x00FF00,  /*stand=*/0xFFFF00, /*limit=*/0xFF5555,
    /*dim=*/0x7A7A7A,
};

constexpr int kTitleBarHeight = 18;
constexpr int kColumnHeaderHeight = 16;
constexpr int kStatusBarHeight = 16;
constexpr int kListMargin = 2;
constexpr int kFramedChromeHeight = kTitleBarHeight + kColumnHeaderHeight +
                                    kStatusBarHeight + kListMargin * 2;

bool win95Theme = false;

const Palette& paletteFor(int64_t nowEpoch) {
    return isNightAt(nowEpoch, kLocalUtcOffsetSeconds) ? kNightPalette
                                                       : kDayPalette;
}

// There is no bold DejaVu on the device, so weight comes from drawing the
// glyphs twice, a pixel apart. Which way it spreads matters: right-aligned
// text has to grow leftward or it walks out of the column it is aligned to.
void drawBoldString(const char* text, int x, int y, bool growLeft) {
    canvas.drawString(text, x, y);
    canvas.drawString(text, growLeft ? x - 1 : x + 1, y);
}

// Two stacked bars, which is about all a 25px row can spare and still read as
// two decks. Drawn only for a double decker: a marker that tries to mean
// single, double and bendy at six pixels across means none of them, and the
// feed's single deck is the unremarkable case anyway.
constexpr int kDeckMarkerWidth = 6;
constexpr int kDeckMarkerHeight = 9;
constexpr int kDeckMarkerGap = 3;

void drawDoubleDeckMarker(int right, int top, uint32_t color) {
    const int x = right - kDeckMarkerWidth;
    canvas.fillRect(x, top, kDeckMarkerWidth, 4, color);
    canvas.fillRect(x, top + 5, kDeckMarkerWidth, 4, color);
}

// A 1px bevel rather than the portal's 2px one: at this size the second pixel
// reads as a smudge instead of a highlight.
void drawBevel(int x, int y, int w, int h, uint32_t topLeft,
               uint32_t bottomRight) {
    canvas.drawFastHLine(x, y, w, topLeft);
    canvas.drawFastVLine(x, y, h, topLeft);
    canvas.drawFastHLine(x, y + h - 1, w, bottomRight);
    canvas.drawFastVLine(x + w - 1, y, h, bottomRight);
}

void drawTitleGradient(int x, int y, int w, int h, const Palette& p) {
    if (p.capA == p.capB) {
        canvas.fillRect(x, y, w, h, p.capA);
        return;
    }
    for (int i = 0; i < w; ++i) {
        const int r = static_cast<int>((((p.capA >> 16) & 0xFF) * (w - i) +
                                        ((p.capB >> 16) & 0xFF) * i) / w);
        const int g = static_cast<int>((((p.capA >> 8) & 0xFF) * (w - i) +
                                        ((p.capB >> 8) & 0xFF) * i) / w);
        const int b = static_cast<int>(((p.capA & 0xFF) * (w - i) +
                                        (p.capB & 0xFF) * i) / w);
        canvas.drawFastVLine(
            x + i, y, h,
            static_cast<uint32_t>((r << 16) | (g << 8) | b));
    }
}

// The 96x96 source bitmap has blank padding around the glyph; only rows
// 16..83 carry ink, so the rest is cropped before scaling.
constexpr int kWifiIconRowBytes = WIFIIMAGE_WIDTH / 8;
constexpr int kWifiIconInkTop = 16;
constexpr int kWifiIconInkHeight = 68;
constexpr float kWifiIconScale = 0.25f;
constexpr int kWifiIconTextGap = 12;

// The service rows and the header end a few pixels short of the bottom edge,
// leaving room for an indicator that shows which page of a long service list
// is on screen.
void drawPageDots(size_t currentPage, size_t totalPages, int pageDotsY) {
    constexpr int kDotRadius = 2;
    constexpr int kDotSpacing = 8;

    int y = pageDotsY;
    int x = screenWidth() / 2 -
            (static_cast<int>(totalPages - 1) * kDotSpacing) / 2;
    for (size_t i = 0; i < totalPages; ++i) {
        if (i == currentPage) {
            canvas.fillCircle(x, y, kDotRadius, TFT_WHITE);
        } else {
            canvas.drawCircle(x, y, kDotRadius, TFT_WHITE);
        }
        x += kDotSpacing;
    }
}

constexpr int kBatteryBodyWidth = 20;
constexpr int kBatteryHeight = 11;
constexpr int kBatteryTipWidth = 2;
constexpr int kBatteryTipHeight = 5;
constexpr int kBatteryRightMargin = 2;
constexpr int kBatteryY = 2;
constexpr int kBatteryLowPercent = 20;
// The header is centred in what the battery icon leaves free, so a long stop
// name cannot run underneath it.
constexpr int kHeaderRightPad =
    kBatteryBodyWidth + kBatteryTipWidth + kBatteryRightMargin + 4;

void drawBattery(const hal::PowerStatus& power) {
    if (power.percent < 0) {
        return;  // nothing read yet, or no gauge on this board
    }

    int x = screenWidth() - kBatteryBodyWidth - kBatteryTipWidth -
            kBatteryRightMargin;
    canvas.drawRect(x, kBatteryY, kBatteryBodyWidth, kBatteryHeight, TFT_WHITE);
    canvas.fillRect(x + kBatteryBodyWidth,
                    kBatteryY + (kBatteryHeight - kBatteryTipHeight) / 2,
                    kBatteryTipWidth, kBatteryTipHeight, TFT_WHITE);

    if (power.percent == 0) {
        return;
    }
    int innerWidth = kBatteryBodyWidth - 2;
    // Keep a sliver visible at low percentages so it stays distinguishable
    // from an empty outline.
    int fillWidth = (innerWidth * power.percent + 50) / 100;
    if (fillWidth < 1) {
        fillWidth = 1;
    }

    uint16_t color = TFT_WHITE;
    if (power.charging) {
        color = TFT_GREEN;
    } else if (power.percent <= kBatteryLowPercent) {
        color = TFT_RED;
    }
    canvas.fillRect(x + 1, kBatteryY + 1, fillWidth, kBatteryHeight - 2, color);
}

// The framed screen's own battery, sitting here beside the plain one rather
// than up with the rest of the Windows 95 code so that both renderers of the
// same widget stay together -- and so this one can read the shared
// kBatteryLowPercent above. It is a separate function because the plain
// screen's icon is positioned and coloured for the top right, and the two
// boards without the theme must keep it exactly as it is.
void drawStatusBattery(int right, int centerY, const hal::PowerStatus& power,
                       const Palette& p) {
    if (power.percent < 0) {
        return;
    }
    constexpr int kW = 18;
    constexpr int kH = 9;
    const int x = right - kW - 2;
    const int y = centerY - kH / 2;

    canvas.drawRect(x, y, kW, kH, p.ink);
    canvas.fillRect(x + kW, y + 2, 2, kH - 4, p.ink);

    int fill = (kW - 2) * power.percent / 100;
    if (fill < 1 && power.percent > 0) {
        fill = 1;
    }
    uint32_t tint = p.seats;
    if (power.percent <= kBatteryLowPercent) {
        tint = p.limit;
    }
    if (fill > 0) {
        canvas.fillRect(x + 1, y + 1, fill, kH - 2, tint);
    }
}

// The framed screen, drawn top to bottom: title bar, column header, sunken
// list, status bar. Everything is positioned from the chrome constants rather
// than the shared ArrivalsLayout, which only supplies the row pitch and the
// ETA columns.
void drawFramedArrivals(const std::string& stopLabel,
                        const std::vector<BusServiceRow>& rows,
                        int64_t nowEpoch, size_t currentStopIndex,
                        size_t totalStops, size_t currentPage,
                        size_t totalPages, const hal::PowerStatus& power,
                        uint32_t dataAgeMs) {
    // Check if any row has a label, then select the appropriate layout
    bool hasLabels = false;
    for (const BusServiceRow& row : rows) {
        if (!row.label.empty()) {
            hasLabels = true;
            break;
        }
    }
    
    const ArrivalsLayout& layout = hasLabels ? arrivalsLayoutTwoLine : arrivalsLayoutSingleLine;
    
    const Palette& p = paletteFor(nowEpoch);
    const int w = screenWidth();
    const int h = screenHeight();

    canvas.fillSprite(p.chrome);

    // Title bar.
    drawTitleGradient(0, 0, w, kTitleBarHeight, p);
    if (p.capA == p.capB) {
        // High contrast has no gradient to separate the bar, so it gets a rule.
        canvas.drawFastHLine(0, kTitleBarHeight - 1, w, p.hi);
    }
    canvas.setFont(&fonts::DejaVu12);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(p.capInk);
    
    // Build title with age indicator, truncated to end before buttons (x=276)
    constexpr int kTitleMaxX = 276;
    constexpr int kTitleStartX = 4;
    int titleMaxWidth = kTitleMaxX - kTitleStartX;
    
    std::string title = stopLabel + "  " +
                       std::to_string(currentStopIndex + 1) + " of " +
                       std::to_string(totalStops);
    
    // Append age if data is stale (> 60s)
    if (dataAgeMs > 60000) {
        uint32_t ageSec = dataAgeMs / 1000;
        char ageBuf[16];
        if (ageSec < 120) {
            std::snprintf(ageBuf, sizeof(ageBuf), " %us", static_cast<unsigned>(ageSec));
        } else {
            std::snprintf(ageBuf, sizeof(ageBuf), " %um", static_cast<unsigned>(ageSec / 60));
        }
        title += ageBuf;
    }
    
    // Truncate title if needed
    if (canvas.textWidth(title.c_str()) > titleMaxWidth) {
        title = truncateText(title, titleMaxWidth, [](const char* s) {
            return canvas.textWidth(s);
        });
    }
    
    drawBoldString(title.c_str(), kTitleStartX, 3, /*growLeft=*/false);

    // Window buttons, right to left, so they stay put as the title grows.
    int bx = w - 3 - 13;
    const char* glyphs[] = {"x", "[]", "_"};
    for (const char* glyph : glyphs) {
        canvas.fillRect(bx, 3, 13, 12, p.chrome);
        drawBevel(bx, 3, 13, 12, p.hi, p.lo);
        canvas.setTextColor(p.ink);
        canvas.setTextDatum(middle_center);
        canvas.drawString(glyph, bx + 7, 9);
        canvas.setTextDatum(top_left);
        bx -= 14;
    }

    // Column header. Names what the three numbers are, which the plain screen
    // never says.
    const int hdrY = kTitleBarHeight;
    canvas.fillRect(0, hdrY, w, kColumnHeaderHeight, p.chrome);
    drawBevel(0, hdrY, w, kColumnHeaderHeight, p.hi, p.lo);
    canvas.setTextColor(p.ink);
    canvas.drawString("Service", layout.serviceColX + 2, hdrY + 3);
    const char* colNames[] = {"Next", "Then", "Then"};
    canvas.setTextDatum(top_right);
    for (size_t c = 0; c < kArrivalsPerService; ++c) {
        canvas.drawString(colNames[c], layout.etaColRightX[c],
                          hdrY + 3);
    }

    // Sunken list.
    const int listY = hdrY + kColumnHeaderHeight;
    const int listH = h - listY - kStatusBarHeight;
    canvas.fillRect(0, listY, w, listH, p.face);
    drawBevel(0, listY, w, listH, p.lo, p.hi);

    canvas.setFont(arrivalsFont());
    const int rowHeight = layout.rowHeight;
    for (size_t i = 0;
         i < rows.size() && i < layout.servicesPerScreen; ++i) {
        const int y = listY + kListMargin + static_cast<int>(i) * rowHeight;
        const BusServiceRow& row = rows[i];

        canvas.setFont(arrivalsFont());
        canvas.setTextColor(p.ink);
        canvas.setTextDatum(top_left);
        drawBoldString(row.serviceNo.c_str(), layout.serviceColX + 2,
                       y, /*growLeft=*/false);

        // Draw label below service number if present (Win95 theme)
        if (!row.label.empty()) {
            const int labelX = layout.serviceColX + 2;
            const int labelY = y + (board().arrivalsFontHeight >= 24 ? 20 : 15);
            const int maxLabelWidth = screenWidth() - labelX - 8;
            
#if BUS_AUNTY_SHOW_VISIT_MARKER
            // Check if this row has any visit-2 arrivals to determine marker reservation
            bool hasVisit2 = false;
            for (size_t col = 0; col < kArrivalsPerService; ++col) {
                if (row.arrivals[col].visitNumber == "2" && row.arrivals[col].etaEpoch >= 0) {
                    hasVisit2 = true;
                    break;
                }
            }
#else
            bool hasVisit2 = false;
#endif
            
            canvas.setFont(labelFont());
            std::string truncated = truncateLabel(row.label, maxLabelWidth, labelFont(), hasVisit2);
            canvas.setTextColor(p.ink);
            canvas.drawString(truncated.c_str(), labelX, labelY);
            
#if BUS_AUNTY_SHOW_VISIT_MARKER
            // Add "2nd" marker for second-visit arrivals
            if (hasVisit2) {
                int markerX = labelX + canvas.textWidth(truncated.c_str());
                canvas.drawString(" 2nd", markerX, labelY);
            }
#endif
        }

        canvas.setFont(arrivalsFont());
        canvas.setTextDatum(top_right);
        for (size_t col = 0; col < kArrivalsPerService; ++col) {
            const BusArrival& arrival = row.arrivals[col];
            const std::string eta = formatEtaMinutes(arrival.etaEpoch,
                                                     nowEpoch);
            uint32_t tint = p.dim;
            switch (arrival.load) {
                case BusLoad::SeatsAvailable: tint = p.seats; break;
                case BusLoad::StandingAvailable: tint = p.stand; break;
                case BusLoad::LimitedStanding: tint = p.limit; break;
                case BusLoad::Unknown: tint = p.ink; break;
            }
            canvas.setTextColor(tint);
            drawBoldString(eta.c_str(), layout.etaColRightX[col], y,
                           /*growLeft=*/true);
            if (eta == kEtaArrivingLabel) {
                // One pass more than the rest of the row. An arriving bus was
                // emphasised by being the only bold thing here; now that
                // everything is bold it needs the extra to keep saying
                // anything, and colour is already spoken for by load.
                canvas.drawString(eta.c_str(),
                                  layout.etaColRightX[col] - 2, y);
            }

            // Sits to the left of the time it belongs to, measured rather
            // than assumed because the column is right-aligned and "Arr" is
            // wider than "3".
            if (arrival.type == BusType::DoubleDeck) {
                // Plus the pixel the bold pass spreads leftward, or the gap
                // closes up against a glyph only six pixels wide.
                const int textWidth = canvas.textWidth(eta.c_str()) + 1;
                drawDoubleDeckMarker(
                    layout.etaColRightX[col] - textWidth -
                        kDeckMarkerGap,
                    y + (rowHeight - kDeckMarkerHeight) / 2, tint);
            }
        }
    }

    // Status bar: which page, the time, and the battery.
    const int barY = h - kStatusBarHeight;
    canvas.fillRect(0, barY, w, kStatusBarHeight, p.chrome);
    drawBevel(0, barY, w, kStatusBarHeight, p.hi, p.lo);
    const int barMid = barY + kStatusBarHeight / 2;

    int px = 6;
    for (size_t i = 0; i < totalPages; ++i) {
        if (i == currentPage) {
            canvas.fillRect(px, barMid - 2, 5, 5, p.ink);
        } else {
            canvas.drawRect(px, barMid - 2, 5, 5, p.ink);
        }
        px += 8;
    }

    const int hour = localHourFor(nowEpoch, kLocalUtcOffsetSeconds);
    if (hour >= 0) {
        // Minutes are the same arithmetic one unit down; not worth a second
        // core function for one modulo.
        const int minute = static_cast<int>(
            ((nowEpoch + kLocalUtcOffsetSeconds) % 3600) / 60);
        char clock[6];
        std::snprintf(clock, sizeof(clock), "%02d:%02d", hour, minute);
        canvas.setFont(&fonts::DejaVu12);
        canvas.setTextColor(p.ink);
        canvas.setTextDatum(middle_center);
        canvas.drawString(clock, w / 2, barMid);
    }

    drawStatusBattery(w - 2, barMid, power, p);

    canvas.pushSprite(0, 0);
    canvas.setTextFont(kDefaultTextFont);
    canvas.setTextDatum(top_left);
}

}  // namespace

void displaySetup() {
    hal::displayDeviceBegin();

    canvas.setColorDepth(8);
    canvas.createSprite(screenWidth(), screenHeight());

    displaySetTheme(false);
    canvas.setTextSize(1);
}

void displaySetTheme(bool win95) {
    win95Theme = win95 && board().supportsFramedTheme;

    // The layout is measured with the arrivals font selected, before the
    // shared default goes back on. Measured rather than taken from the
    // profile, because a font's line advance is not its name: DejaVu24
    // advances 25px, and the rows have to be spaced by what is drawn.
    canvas.setFont(arrivalsFont());
    const int rowHeight = canvas.fontHeight();

    // Compute both single-line (no labels) and two-line (with labels) layouts
    if (win95Theme) {
        const int listHeight = screenHeight() - kFramedChromeHeight;
        // computeArrivalsLayout reserves its first row for a header that the
        // framed screen draws as chrome instead, so it is handed one row more
        // than the list really has and hands the right count back.
        arrivalsLayoutSingleLine = computeArrivalsLayout(
            screenWidth(), listHeight + rowHeight, rowHeight, /*battery=*/0, false);
        arrivalsLayoutTwoLine = computeArrivalsLayout(
            screenWidth(), listHeight + rowHeight, rowHeight, /*battery=*/0, true);
    } else {
        arrivalsLayoutSingleLine = computeArrivalsLayout(
            screenWidth(), screenHeight(), rowHeight, kHeaderRightPad, false);
        arrivalsLayoutTwoLine = computeArrivalsLayout(
            screenWidth(), screenHeight(), rowHeight, kHeaderRightPad, true);
    }

    canvas.setTextFont(kDefaultTextFont);
}

bool displayThemeIsWin95() { return win95Theme; }

size_t servicesPerScreen(bool hasLabels) {
    return hasLabels ? arrivalsLayoutTwoLine.servicesPerScreen
                     : arrivalsLayoutSingleLine.servicesPerScreen;
}

void displaySetDimmed(bool dimmed) {
    hal::displayDeviceSetBrightness(dimmed ? board().brightnessDim
                                           : board().brightnessFull);
}

void displaySleep() {
    // Clear before sleeping: the panel keeps its own frame buffer, so whatever
    // was last pushed would otherwise flash back up on wake, showing arrival
    // times that are by then minutes stale.
    canvas.fillSprite(TFT_BLACK);
    canvas.pushSprite(0, 0);
    hal::displayDeviceSleep();
}

void displayWake() { hal::displayDeviceWake(); }

void displayShowStatus(const std::string& message) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextDatum(middle_center);

    std::vector<std::string> lines;
    size_t start = 0;
    while (true) {
        size_t pos = message.find('\n', start);
        if (pos == std::string::npos) {
            lines.push_back(message.substr(start));
            break;
        }
        lines.push_back(message.substr(start, pos - start));
        start = pos + 1;
    }

    int lineHeight = canvas.fontHeight();
    int totalHeight = lineHeight * static_cast<int>(lines.size());
    int firstLineY = (screenHeight() - totalHeight) / 2 + lineHeight / 2;

    for (size_t i = 0; i < lines.size(); ++i) {
        int y = firstLineY + static_cast<int>(i) * lineHeight;
        canvas.drawString(lines[i].c_str(), screenWidth() / 2, y);
    }

    canvas.pushSprite(0, 0);
}

void displayShowWifiSetup(const std::string& ssid,
                          const std::string& password) {
    canvas.fillSprite(TFT_BLACK);
    // 18px here rather than the shared 16px default. Font 2 is a bitmap font,
    // so scaling it to 18 would resample unevenly; DejaVu18 is natively 18px.
    canvas.setFont(&fonts::DejaVu18);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextDatum(top_center);

    int lineHeight = canvas.fontHeight();
    int iconHeight = static_cast<int>(kWifiIconInkHeight * kWifiIconScale + 0.5f);
    int blockHeight = iconHeight + kWifiIconTextGap + lineHeight * 3;
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

    int textY = iconY + iconHeight + kWifiIconTextGap;
    canvas.drawString("Connect WiFi to:", screenWidth() / 2, textY);
    canvas.drawString(ssid.c_str(), screenWidth() / 2, textY + lineHeight);
    canvas.drawString(password.c_str(), screenWidth() / 2,
                      textY + lineHeight * 2);

    canvas.pushSprite(0, 0);
    canvas.setTextFont(kDefaultTextFont);
}

void displayShowConfig(const std::string& url, const std::string& ip,
                       uint32_t remainingMs) {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextDatum(top_center);

    char remain[24];
    std::snprintf(remain, sizeof(remain), "unlocked %us",
                  static_cast<unsigned>((remainingMs + 999) / 1000));
    const std::string lines[] = {url, ip, remain, "press again for AP"};
    int lineHeight = canvas.fontHeight();
    int count = static_cast<int>(sizeof(lines) / sizeof(lines[0]));
    int firstLineY = (screenHeight() - lineHeight * count) / 2;
    for (int i = 0; i < count; ++i) {
        canvas.drawString(lines[i].c_str(), screenWidth() / 2,
                          firstLineY + i * lineHeight);
    }
    canvas.pushSprite(0, 0);
}

void displayShowWifiOffline() {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);

    // Three lines. The third is the way out of a device that can no longer
    // reach its network at all: without it, the one screen a user is most
    // stuck on was also the only one that never said how to reopen setup.
    MessageLine lines[3];
    lines[0].text = "No WiFi";
    lines[1].text = "Press any button to retry";
    lines[2] = secondaryButtonLine("for 3s to reset WiFi");

    drawCenteredMessage(lines, 3);

    canvas.pushSprite(0, 0);
    canvas.setTextFont(kDefaultTextFont);
}

void displayShowNoStops() {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);

    // Four lines rather than three: "Then go to busaunty.local in your
    // browser" is too long to set on one line at a size worth reading, on any
    // of the three panels.
    MessageLine lines[4];
    lines[0].text = "No Bus Stops Configured";
    lines[1] = secondaryButtonLine("for 3s");
    lines[2].text = "Then go to busaunty.local";
    lines[3].text = "in your browser";

    drawCenteredMessage(lines, 4);

    canvas.pushSprite(0, 0);
    canvas.setTextFont(kDefaultTextFont);
}

void displayShowArrivals(const std::string& stopLabel,
                          const std::vector<BusServiceRow>& rows,
                          int64_t nowEpoch, size_t currentStopIndex,
                          size_t totalStops, size_t currentPage,
                          size_t totalPages, const hal::PowerStatus& power,
                          uint32_t dataAgeMs) {
    if (win95Theme) {
        drawFramedArrivals(stopLabel, rows, nowEpoch, currentStopIndex,
                           totalStops, currentPage, totalPages, power, dataAgeMs);
        return;
    }

    canvas.fillSprite(TFT_BLACK);
    canvas.setFont(arrivalsFont());
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);

    // Check if any row has a label, then select the appropriate layout
    bool hasLabels = false;
    for (const BusServiceRow& row : rows) {
        if (!row.label.empty()) {
            hasLabels = true;
            break;
        }
    }
    
    const ArrivalsLayout& layout = hasLabels ? arrivalsLayoutTwoLine : arrivalsLayoutSingleLine;
    const int rowHeight = layout.rowHeight;

    canvas.setTextDatum(top_center);
    canvas.setFont(arrivalsFont());
    
    // Build header with intelligent truncation
    int maxHeaderWidth = screenWidth() - kHeaderRightPad - 4;
    std::string header = buildHeader(stopLabel, currentStopIndex, totalStops,
                                    dataAgeMs, maxHeaderWidth,
                                    [](const char* s) { return canvas.textWidth(s); });
    
    canvas.drawString(header.c_str(), layout.headerCenterX, 0);

    drawBattery(power);
    
    for (size_t i = 0;
         i < rows.size() && i < layout.servicesPerScreen; ++i) {
        int y = rowHeight + static_cast<int>(i) * rowHeight;
        const BusServiceRow& row = rows[i];

        canvas.setFont(arrivalsFont());
        canvas.setTextColor(TFT_WHITE, TFT_BLACK);
        canvas.setTextDatum(top_left);
        canvas.drawString(row.serviceNo.c_str(), layout.serviceColX, y);

        // Draw label below service number if present
        if (!row.label.empty()) {
            const int labelX = layout.serviceColX;
            const int labelY = y + (board().arrivalsFontHeight >= 24 ? 20 : 15);
            const int maxLabelWidth = board().screenWidth - labelX - 
                                      (board().arrivalsFontHeight >= 24 ? 8 : 8);
            
#if BUS_AUNTY_SHOW_VISIT_MARKER
            // Check if this row has any visit-2 arrivals to determine marker reservation
            bool hasVisit2 = false;
            for (size_t col = 0; col < kArrivalsPerService; ++col) {
                if (row.arrivals[col].visitNumber == "2" && row.arrivals[col].etaEpoch >= 0) {
                    hasVisit2 = true;
                    break;
                }
            }
#else
            bool hasVisit2 = false;
#endif
            
            canvas.setFont(labelFont());
            std::string truncated = truncateLabel(row.label, maxLabelWidth, labelFont(), hasVisit2);
            canvas.drawString(truncated.c_str(), labelX, labelY);
            
#if BUS_AUNTY_SHOW_VISIT_MARKER
            // Add "2nd" marker for second-visit arrivals
            if (hasVisit2) {
                int markerX = labelX + canvas.textWidth(truncated.c_str());
                canvas.drawString(" 2nd", markerX, labelY);
            }
#endif
        }

        canvas.setFont(arrivalsFont());
        canvas.setTextDatum(top_right);
        for (size_t col = 0; col < kArrivalsPerService; ++col) {
            const BusArrival& arrival = row.arrivals[col];
            std::string eta = formatEtaMinutes(arrival.etaEpoch, nowEpoch);

            // Single argument leaves the text background transparent, which
            // the second pass below depends on. Safe because every frame
            // starts from a cleared sprite.
            canvas.setTextColor(loadColor(arrival.load));
            canvas.drawString(eta.c_str(), layout.etaColRightX[col], y);

            // Colour is spoken for by load, so an arriving bus is emphasised
            // by weight: overdrawing a pixel to the left thickens the stems.
            // Leftward because the columns are right-aligned, so that is
            // where the spare room is.
            if (eta == kEtaArrivingLabel) {
                canvas.drawString(eta.c_str(),
                                  layout.etaColRightX[col] - 1, y);
            }
        }
    }

    if (totalPages > 1) {
        drawPageDots(currentPage, totalPages, layout.pageDotsY);
    }

    canvas.pushSprite(0, 0);
    canvas.setTextFont(kDefaultTextFont);
}
