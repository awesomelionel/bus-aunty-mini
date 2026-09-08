// firmware/include/core/sleep_policy.h
#pragma once
#include <cstdint>

// Idling goes through two stages before the device shuts down: the backlight
// drops first, which is both a saving on its own and a warning that the screen
// is about to go, and then everything sleeps until a button is pressed.
enum class PowerMode { Awake, Dimmed, Asleep };

struct SleepSettings {
    bool enabled = true;
    uint32_t dimAfterMs = 0;
    uint32_t sleepAfterMs = 0;
};

struct IdleInputs {
    uint32_t nowMs = 0;
    // millis() at the last button press, portal exit, or wake.
    uint32_t lastInteractionMs = 0;
    // Sleeping is only ever worth it on battery, so a device on USB stays lit.
    bool externallyPowered = false;
};

PowerMode nextPowerMode(const SleepSettings& settings, const IdleInputs& inputs);
