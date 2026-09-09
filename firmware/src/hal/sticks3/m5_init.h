// firmware/src/hal/sticks3/m5_init.h
#pragma once

// M5.begin() brings up the panel, the PMIC and the buttons in one call, but
// this board's HAL is split across four translation units and any of them may
// be initialised first. Idempotent, so each can simply ask rather than the
// call order having to be maintained by hand.
//
// Board-private: not under include/, because nothing outside
// src/hal/sticks3/ may depend on M5Unified.
namespace hal::sticks3 {
void ensureM5Begun();
}
