// firmware/src/core/sleep_policy.cpp
#include "core/sleep_policy.h"

PowerMode nextPowerMode(const SleepSettings& settings,
                        const IdleInputs& inputs) {
    if (!settings.enabled || inputs.externallyPowered) {
        return PowerMode::Awake;
    }

    // Unsigned subtraction, so this stays right across the millis() rollover
    // at 49.7 days rather than idling forever from that point on.
    uint32_t idleMs = inputs.nowMs - inputs.lastInteractionMs;

    return idleMs >= settings.sleepAfterMs ? PowerMode::Asleep
                                           : PowerMode::Awake;
}
