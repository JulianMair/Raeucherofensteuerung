#include "rof_pi.h"

#include <stdbool.h>
#include <stddef.h>

/* Größte Regelabweichung, die noch ein Messwert sein kann. Alles darüber,
 * auch NaN und unendlich, ist ein Fehler und ergibt Tastgrad 0. */
#define MAX_ERROR_C 1000.0f

rof_pi_config_t rof_pi_default_config(void)
{
    return (rof_pi_config_t){ .kp = 0.075f, .ti_s = 250.0f };
}

rof_status_t rof_pi_init(rof_pi_t *pi, const rof_pi_config_t *config)
{
    /* So formuliert, dass auch NaN abgelehnt wird. */
    if (pi == NULL || config == NULL || !(config->kp > 0.0f) || !(config->ti_s > 0.0f)) {
        return ROF_ERR_INVALID_ARG;
    }
    *pi = (rof_pi_t){ .config = *config, .integral = 0.0f };
    return ROF_OK;
}

void rof_pi_reset(rof_pi_t *pi)
{
    pi->integral = 0.0f;
}

float rof_pi_update(rof_pi_t *pi, float setpoint_c, float measured_c, float dt_s)
{
    const rof_pi_config_t *c = &pi->config;
    const float error_c = setpoint_c - measured_c;
    if (!(error_c >= -MAX_ERROR_C && error_c <= MAX_ERROR_C)) {
        return 0.0f; /* der I-Anteil bleibt unberührt */
    }
    if (!(dt_s > 0.0f)) {
        dt_s = 0.0f; /* kein gültiger Zeitschritt: nichts integrieren */
    }

    const float p = c->kp * error_c;
    const float integral_next = pi->integral + c->kp * error_c * dt_s / c->ti_s;

    /* Anti-Windup: am Anschlag nicht weiter in dieselbe Richtung integrieren. */
    const float unlimited = p + integral_next;
    const bool pushes_above_one = unlimited > 1.0f && error_c > 0.0f;
    const bool pushes_below_zero = unlimited < 0.0f && error_c < 0.0f;
    if (!pushes_above_one && !pushes_below_zero) {
        pi->integral = integral_next;
    }

    const float duty = p + pi->integral;
    if (duty < 0.0f) {
        return 0.0f;
    }
    if (duty > 1.0f) {
        return 1.0f;
    }
    return duty;
}
