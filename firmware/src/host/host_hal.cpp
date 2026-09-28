#include "host/host_gfx.h"

#include "hal/display_device.h"

namespace hal {

namespace {
HostGfx gGfx;
}

Gfx& gfx() { return gGfx; }

void displayDeviceBegin() {}
void displayDeviceSetBrightness(uint8_t) {}
void displayDeviceSleep() {}
void displayDeviceWake() {}

}  // namespace hal
