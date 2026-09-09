// firmware/include/hal/gfx_types.h
#pragma once

// The only board #ifdef in the tree. Every other board difference is handled
// by compiling a different file out of src/hal/<board>/, but a shared include
// path cannot resolve to two different headers any other way.
//
// M5GFX is a fork of LovyanGFX, so the two aliases below share an API: the
// same sprite class, the same `fonts::` set, the same drawString/setTextDatum/
// pushRotateZoomWithAA, and the same TFT_* colour macros. That is what lets
// ui/display.cpp be written once against these names.
#if defined(BOARD_STICKS3)
#include <M5Unified.h>
namespace hal {
using Gfx = M5GFX;
using Canvas = M5Canvas;
}  // namespace hal
#elif defined(BOARD_FEATHER_S3_REVTFT)
#include <LovyanGFX.hpp>
namespace hal {
using Gfx = LGFX_Device;
using Canvas = LGFX_Sprite;
}  // namespace hal
#else
#error "No BOARD_* define; see platformio.ini"
#endif
