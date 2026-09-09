// firmware/include/hal/sleep.h
#pragma once

namespace hal {

// Light-sleeps the SoC until one of the board's buttons is pressed, then
// returns.
//
// Light sleep rather than deep sleep: RAM and the peripherals' state survive,
// so the caller keeps its configured stops and cached arrivals, and the RTC
// keeps counting, so the NTP-synced clock is still right on the other side.
// Deep sleep would save a further ~200uA but cost a full boot, reconnect and
// re-sync on every glance at the screen.
//
// Blocking, and it swallows the press that woke the device so a wake is not
// also read as a stop change. The caller is responsible for the screen and
// the radio: neither is touched here.
void sleepUntilButtonPress();

}  // namespace hal
