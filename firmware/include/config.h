#pragma once

// Feature flags for optional firmware features.
// These can be overridden at build time via platformio.ini build_flags.

// Show "2nd" marker for loop second-visit arrivals
#ifndef BUS_AUNTY_SHOW_VISIT_MARKER
#define BUS_AUNTY_SHOW_VISIT_MARKER 1
#endif
