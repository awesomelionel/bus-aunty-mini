// firmware/include/hal/power.h
#pragma once

namespace hal {

// What the board can tell us about its own supply. `externalPower` is derived
// by the board rather than inferred by the caller, because how you know a
// device is on mains differs per board and one of them cannot tell at all.
struct PowerStatus {
    int percent = -1;  // -1 when unknown, or when this board has no gauge
    bool charging = false;
    bool externalPower = false;
};

void powerBegin();
// Samples at a fixed interval; cheap enough to call every loop.
void powerPoll();
PowerStatus powerStatus();

}  // namespace hal
