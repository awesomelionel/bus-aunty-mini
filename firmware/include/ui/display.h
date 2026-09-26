// firmware/include/ui/display.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/arrival_parser.h"
#include "hal/power.h"

void displaySetup();

// Switches the arrivals screen between the plain list and the framed Windows
// 95 treatment, and recomputes the layout for it. The chrome costs rows, so
// servicesPerScreen() changes here and callers holding a page index have to
// re-page. Ignored on boards whose panel is too short for the chrome, which
// say so through BoardProfile::supportsFramedTheme.
void displaySetTheme(bool win95);
bool displayThemeIsWin95();
// How many service rows fit under the header at the arrivals font size.
// Depends on the panel's height and the font's measured row height, so it is
// only meaningful after displaySetup().
size_t servicesPerScreen();
// Drops the backlight to the dim level, or takes it back to full. Whatever is
// on screen stays on screen and stays live.
void displaySetDimmed(bool dimmed);
// Blanks the panel and puts it in its own low-power state. Nothing drawn while
// asleep reaches the screen, so callers should not bother drawing.
void displaySleep();
void displayWake();
void displayShowStatus(const std::string& message);
void displayShowWifiSetup(const std::string& ssid, const std::string& password);
void displayShowConfig(const std::string& url, const std::string& ip,
                       uint32_t remainingMs);
void displayShowWifiOffline();
void displayShowNoStops();
// `stopLabel` is the stop's name, or its code when it was left unnamed.
// `rows` is already the slice for `currentPage`.
void displayShowArrivals(const std::string& stopLabel,
                          const std::vector<BusServiceRow>& rows,
                          int64_t nowEpoch, size_t currentStopIndex,
                          size_t totalStops, size_t currentPage,
                          size_t totalPages, const hal::PowerStatus& power);
