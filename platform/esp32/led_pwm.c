#include "led_pwm.h"

#include "rof_slow_pwm.h"

void led_pwm_init(led_pwm_t *pwm, uint32_t period_ms, uint32_t hold_ms)
{
    *pwm = (led_pwm_t){ .period_ms = period_ms, .hold_ms = hold_ms, .has_duty = false };
}

rof_status_t led_pwm_set_duty(led_pwm_t *pwm, float duty, uint64_t now_ms)
{
    /* So formuliert, dass auch NaN abgelehnt wird */
    if (!(duty >= 0.0f && duty <= 1.0f)) {
        return ROF_ERR_INVALID_ARG;
    }
    pwm->has_duty = true;
    pwm->duty = duty;
    pwm->set_ms = now_ms;
    return ROF_OK;
}

bool led_pwm_is_on(const led_pwm_t *pwm, uint64_t now_ms)
{
    if (!pwm->has_duty) {
        return false;
    }
    /* now_ms kann knapp vor set_ms liegen, wenn der Timer die Zeit vor dem
     * Setzen gelesen hat. Das zählt als frisch. */
    if (now_ms > pwm->set_ms && now_ms - pwm->set_ms > pwm->hold_ms) {
        return false;
    }
    return rof_slow_pwm_on(pwm->period_ms, pwm->duty, now_ms);
}
