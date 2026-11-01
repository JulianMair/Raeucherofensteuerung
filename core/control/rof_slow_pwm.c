#include "rof_slow_pwm.h"

bool rof_slow_pwm_on(uint32_t period_ms, float duty, uint64_t now_ms)
{
    /* So formuliert, dass auch NaN ausschaltet. */
    if (period_ms == 0 || !(duty > 0.0f)) {
        return false;
    }
    if (duty >= 1.0f) {
        return true;
    }
    /* Wie weit sind wir im aktuellen Fenster? An ist der vordere Teil. */
    const uint64_t in_window_ms = now_ms % period_ms;
    const float on_ms = duty * (float)period_ms;
    return (float)in_window_ms < on_ms;
}
