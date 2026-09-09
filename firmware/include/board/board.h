// firmware/include/board/board.h
#pragma once
#include <cstdint>

// Everything the shared UI and main loop need to know about the board they
// happen to be running on. One instance per board, defined in
// src/hal/<board>/board.cpp and selected at build time by build_src_filter,
// so the shared code reads values instead of testing #ifdefs.
struct BoardProfile {
    const char* name;

    // Landscape dimensions, i.e. after `rotation` is applied. Both currently
    // supported panels are 240x135, but the layout is derived from these
    // rather than assuming it.
    int screenWidth;
    int screenHeight;
    uint8_t rotation;

    // Backlight levels, 0-255. Full is set explicitly at boot rather than
    // left to the driver's default so that undimming has a known level to
    // return to. The dim level is low enough to be a clear saving and a
    // visible warning that the screen is about to go, but still readable
    // indoors.
    uint8_t brightnessFull;
    uint8_t brightnessDim;

    // What to call the buttons on screen: "Btn A"/"Btn B" vs "D1"/"D2".
    const char* primaryButtonLabel;
    const char* secondaryButtonLabel;

    // True when the board has a dedicated button that sleeps on a single
    // press, in addition to the hold-the-primary-button gesture.
    bool hasSleepButton;

    // False when the board cannot distinguish an absent battery from a full
    // one, so the sleep gate must not read percent < 0 as "on USB".
    bool detectsBatteryPresence;
};

const BoardProfile& board();
