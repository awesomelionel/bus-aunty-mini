// firmware/include/board/board.h
#pragma once
#include <cstdint>

// Which way a triangle should point to single out the secondary button on
// screen. `None` keeps the written label, which is all a board needs when its
// buttons are legibly named; a direction is for boards where pointing at the
// button beats naming it.
//
// Drawn rather than typed: the DejaVu fonts stop at 0x7e, so there is no arrow
// character available, and a triangle scales with the font for free.
enum class ButtonArrow { None, Up, Down, Left, Right };

// Everything the shared UI and main loop need to know about the board they
// happen to be running on. One instance per board, defined in
// src/hal/<board>/board.cpp and selected at build time by build_src_filter,
// so the shared code reads values instead of testing #ifdefs.
//
// What buttons a board has is not here: that is answered at runtime by
// hal::hasButton(), so there is only ever one source of truth for it.
struct BoardProfile {
    const char* name;

    // Landscape dimensions, i.e. after `rotation` is applied. Two of the
    // supported panels are 240x135 and one is 320x170, so the layout is
    // derived from these rather than assuming either.
    int screenWidth;
    int screenHeight;
    uint8_t rotation;

    // Which DejaVu size the arrivals rows are drawn in, named by its nominal
    // height: 18 on the 240x135 panels, 24 where there is room for it.
    //
    // The row pitch is still measured from the live font rather than taken
    // from here, because a font's line advance is not its name -- DejaVu24
    // advances 25px -- and the layout has to agree with what is drawn.
    uint8_t arrivalsFontHeight;

    // Backlight levels, 0-255. Full is set explicitly at boot rather than
    // left to the driver's default so that undimming has a known level to
    // return to. The dim level is low enough to be a clear saving and a
    // visible warning that the screen is about to go, but still readable
    // indoors.
    uint8_t brightnessFull;
    uint8_t brightnessDim;

    // What to call the button that reopens the setup portal. It is the only
    // button the UI ever has to ask for by name, and how you identify it
    // differs: a board with legible labels on the case can use one, while a
    // board whose silkscreen is on the back has to be described by position.
    const char* secondaryButtonLabel;

    // How that same button is pointed at when there is room to draw instead of
    // spell. A board leaving this None is named by `secondaryButtonLabel`
    // everywhere, exactly as before this field existed.
    ButtonArrow secondaryButtonArrow;

    // False when the board cannot distinguish an absent battery from a full
    // one, so the sleep gate must not read percent < 0 as "on USB".
    bool detectsBatteryPresence;

    // True when the panel has height to spend on a framed arrivals screen --
    // title bar, column header, status bar -- and still show a useful number
    // of services. The 240x135 boards do not: the same chrome would leave
    // them two rows out of six, so the config page never offers it there.
    bool supportsFramedTheme;
};

const BoardProfile& board();
