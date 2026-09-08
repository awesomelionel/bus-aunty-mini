// firmware/include/core/sleep_policy.h
#pragma once
#include <cstdint>

// A device left alone goes dark: once it has been idle long enough the screen
// and the radio both go off until a button is pressed. There is no dimmed
// in-between stage — the backlight is either at its working level or the
// device is asleep.
enum class PowerMode { Awake, Asleep };

struct SleepSettings {
    bool enabled = true;
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
