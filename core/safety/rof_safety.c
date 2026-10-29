#include "rof_safety.h"

#include <stdbool.h>
#include <stddef.h>

/* ---- Konfiguration ------------------------------------------------------ */

/* Alle Vergleiche sind so formuliert, dass ein NaN die Konfiguration
 * ungültig macht: Jeder Vergleich mit NaN ist falsch. */
static bool config_is_valid(const rof_safety_config_t *c)
{
    return c->plausible_min_c < c->plausible_max_c &&
           c->setpoint_min_c < c->setpoint_max_c &&
           c->setpoint_min_c >= c->plausible_min_c &&
           c->setpoint_max_c <= c->plausible_max_c &&
           c->setpoint_max_c < ROF_SAFETY_HARD_MAX_C &&
           c->freeze_timeout_ms > 0 &&
           c->freeze_min_change_c > 0.0f &&
           c->cycle_window_ms > 0;
}

/* ---- Öffentliche Funktionen --------------------------------------------- */

rof_safety_config_t rof_safety_default_config(void)
{
    return (rof_safety_config_t){
        .plausible_min_c = -20.0f,
        .plausible_max_c = 125.0f,
        .setpoint_min_c = 20.0f,
        .setpoint_max_c = 110.0f,
        .freeze_timeout_ms = 10u * 60u * 1000u,
        .freeze_min_change_c = 0.05f,
        .cycle_window_ms = 3000u,
    };
}

rof_status_t rof_safety_init(rof_safety_t *safety, const rof_safety_config_t *config,
                             const rof_clock_t *clk, const rof_log_t *logger)
{
    if (safety == NULL || config == NULL || clk == NULL || clk->now_ms == NULL ||
        logger == NULL || logger->write == NULL) {
        return ROF_ERR_INVALID_ARG;
    }
    if (!config_is_valid(config)) {
        return ROF_ERR_INVALID_ARG;
    }
    *safety = (rof_safety_t){ .config = *config, .clk = clk, .logger = logger };
    return ROF_OK;
}

rof_setpoint_check_t rof_safety_check_setpoint(const rof_safety_t *safety, float setpoint_c)
{
    const rof_safety_config_t *c = &safety->config;
    if (setpoint_c >= c->setpoint_min_c && setpoint_c <= c->setpoint_max_c) {
        return ROF_SETPOINT_OK;
    }
    if (setpoint_c < c->setpoint_min_c) {
        return ROF_SETPOINT_TOO_LOW;
    }
    if (setpoint_c > c->setpoint_max_c) {
        return ROF_SETPOINT_TOO_HIGH;
    }
    /* Jeder Vergleich mit NaN ist falsch, also bleibt hier nur NaN übrig.
     * Bewusst ohne isnan(): Das Makro erzeugt je nach Plattform Zweige
     * für double und long double, die ein float nie nimmt (E-017). */
    return ROF_SETPOINT_NOT_A_NUMBER;
}
