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

/* ---- Regeln ------------------------------------------------------------- */

/* Logtext je Ursache. Eine Tabelle statt switch: keine Zweige, die die
 * Abdeckungsmessung zählen müsste. */
static const char *const FAULT_TEXT[] = {
    [ROF_FAULT_NONE] = "",
    [ROF_FAULT_CHAMBER_UNASSIGNED] = "Sicherer Zustand: Garraum keinem Sensor zugeordnet",
    [ROF_FAULT_CHAMBER_MISSING] = "Sicherer Zustand: Garraumsensor fehlt",
    [ROF_FAULT_CHAMBER_READ_ERROR] = "Sicherer Zustand: Lesefehler des Garraumsensors",
    [ROF_FAULT_CHAMBER_DUPLICATE] = "Sicherer Zustand: ID des Garraumsensors doppelt",
    [ROF_FAULT_IMPLAUSIBLE] = "Sicherer Zustand: Garraumwert unplausibel",
    [ROF_FAULT_OVER_TEMPERATURE] = "Sicherer Zustand: Übertemperatur im Garraum",
};

/* SAF-07 und SAF-03: Rollenstatus des Garraums. Ein unbekannter Status
 * gilt als Lesefehler, damit er nie als gültig durchgeht. */
static rof_fault_t chamber_fault(rof_role_status_t status)
{
    switch (status) {
    case ROF_ROLE_STATUS_OK:
        return ROF_FAULT_NONE;
    case ROF_ROLE_STATUS_UNASSIGNED:
        return ROF_FAULT_CHAMBER_UNASSIGNED;
    case ROF_ROLE_STATUS_MISSING:
        return ROF_FAULT_CHAMBER_MISSING;
    case ROF_ROLE_STATUS_DUPLICATE:
        return ROF_FAULT_CHAMBER_DUPLICATE;
    case ROF_ROLE_STATUS_READ_ERROR:
    default:
        return ROF_FAULT_CHAMBER_READ_ERROR;
    }
}

/* SAF-04. So formuliert, dass auch NaN unplausibel ist. */
static bool is_plausible(const rof_safety_config_t *c, float temp_c)
{
    return temp_c >= c->plausible_min_c && temp_c <= c->plausible_max_c;
}

/* Erste Regel, die auslöst, in der Reihenfolge von rof_fault_t. Ab der
 * Plausibilität ist chamber_temp_c gültig, weil der Status OK ist. */
static rof_fault_t find_fault(const rof_safety_t *safety, const rof_safety_input_t *input)
{
    const rof_fault_t chamber = chamber_fault(input->chamber_status);
    if (chamber != ROF_FAULT_NONE) {
        return chamber;
    }
    if (!is_plausible(&safety->config, input->chamber_temp_c)) {
        return ROF_FAULT_IMPLAUSIBLE;
    }
    /* SAF-06: unabhängig von Konfiguration und Ausgang */
    if (input->chamber_temp_c > ROF_SAFETY_HARD_MAX_C) {
        return ROF_FAULT_OVER_TEMPERATURE;
    }
    return ROF_FAULT_NONE;
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

rof_fault_t rof_safety_evaluate(rof_safety_t *safety, const rof_safety_input_t *input)
{
    const rof_fault_t active = find_fault(safety, input);
    if (active != ROF_FAULT_NONE && safety->fault == ROF_FAULT_NONE) {
        safety->fault = active;
        rof_log_write(safety->logger, ROF_LOG_ERROR, FAULT_TEXT[active]);
    }
    return safety->fault;
}

float rof_safety_limit_duty(const rof_safety_t *safety, float requested_duty)
{
    if (safety->fault != ROF_FAULT_NONE) {
        return 0.0f;
    }
    return requested_duty;
}

rof_fault_t rof_safety_fault(const rof_safety_t *safety)
{
    return safety->fault;
}
