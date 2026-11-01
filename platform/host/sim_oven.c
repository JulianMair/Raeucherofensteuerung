#include "sim_oven.h"

#include "rof_slow_pwm.h"

#include <string.h>

/* ---- Modell ------------------------------------------------------------- */

static size_t to_steps(float seconds)
{
    return (size_t)(seconds * 1000.0f / (float)SIM_OVEN_STEP_MS + 0.5f);
}

/* Index des Werts von vor steps Schritten im Ringpuffer */
static size_t steps_back(size_t pos, size_t steps)
{
    return (pos + SIM_OVEN_MAX_DEAD_STEPS - steps) % SIM_OVEN_MAX_DEAD_STEPS;
}

/* Ein Schritt nach dem expliziten Euler-Verfahren. */
static void step(sim_oven_t *sim)
{
    const sim_oven_config_t *c = &sim->config;
    const double dt_s = (double)SIM_OVEN_STEP_MS / 1000.0;
    const bool heater_on = sim->heater_stuck_on || rof_slow_pwm_on(c->pwm_period_ms, sim->duty, sim->now_ms);

    /* Aktuelle Werte ablegen, dann die verzögerten von vor der Totzeit lesen */
    const size_t pos = sim->history_pos;
    sim->heater_history[pos] = heater_on ? 1u : 0u;
    sim->chamber_history[pos] = sim->chamber_c;
    const double heater_delayed = sim->heater_history[steps_back(pos, sim->chamber_dead_steps)];
    const double chamber_delayed = sim->chamber_history[steps_back(pos, sim->probe_dead_steps)];

    /* Garraum strebt zu Umgebung + Heizleistung, Kern zum Garraum */
    const double chamber_target_c = c->ambient_c + c->heater_gain_c * heater_delayed;
    sim->chamber_c += (chamber_target_c - sim->chamber_c) * dt_s / c->chamber_tau_s;
    sim->probe_c += (chamber_delayed - sim->probe_c) * dt_s / c->probe_tau_s;

    sim->history_pos = (pos + 1) % SIM_OVEN_MAX_DEAD_STEPS;
    sim->now_ms += SIM_OVEN_STEP_MS;
}

/* ---- Interfaces --------------------------------------------------------- */

static rof_status_t sim_enumerate(void *ctx, rof_sensor_info_t *list, size_t max, size_t *count)
{
    const sim_oven_t *sim = ctx;
    const rof_sensor_id_t present[2] = { SIM_OVEN_CHAMBER_ID, SIM_OVEN_PROBE_ID };
    *count = 0;
    for (size_t i = 0; i < 2; i++) {
        if (present[i] == SIM_OVEN_CHAMBER_ID && sim->chamber_unplugged) {
            continue;
        }
        if (*count == max) {
            return ROF_ERR_NO_SPACE;
        }
        list[*count].id = present[i];
        list[*count].type = "SIM";
        (*count)++;
    }
    return ROF_OK;
}

static rof_status_t sim_read(void *ctx, rof_sensor_id_t id, float *temp_c)
{
    const sim_oven_t *sim = ctx;
    if (id == SIM_OVEN_CHAMBER_ID && !sim->chamber_unplugged) {
        *temp_c = sim->chamber_frozen ? sim->frozen_c : (float)sim->chamber_c;
        return ROF_OK;
    }
    if (id == SIM_OVEN_PROBE_ID) {
        *temp_c = (float)sim->probe_c;
        return ROF_OK;
    }
    return ROF_ERR_NOT_FOUND;
}

static rof_status_t sim_set_duty(void *ctx, float duty)
{
    sim_oven_t *sim = ctx;
    /* So formuliert, dass auch NaN abgelehnt wird, wie bei einem echten Ausgang */
    if (!(duty >= 0.0f && duty <= 1.0f)) {
        return ROF_ERR_INVALID_ARG;
    }
    sim->duty = duty;
    return ROF_OK;
}

static uint64_t sim_now_ms(void *ctx)
{
    const sim_oven_t *sim = ctx;
    return sim->now_ms;
}

/* ---- Öffentliche Funktionen --------------------------------------------- */

sim_oven_config_t sim_oven_default_config(void)
{
    return (sim_oven_config_t){
        .ambient_c = 20.0f,
        .heater_gain_c = 130.0f,
        .chamber_tau_s = 600.0f,
        .chamber_dead_s = 30.0f,
        .probe_tau_s = 3600.0f,
        .probe_dead_s = 120.0f,
        .pwm_period_ms = 4000u,
    };
}

rof_status_t sim_oven_init(sim_oven_t *sim, const sim_oven_config_t *config)
{
    const sim_oven_config_t *c = config;
    if (!(c->chamber_tau_s > 0.0f) || !(c->probe_tau_s > 0.0f) || !(c->chamber_dead_s >= 0.0f) ||
        !(c->probe_dead_s >= 0.0f) || to_steps(c->chamber_dead_s) >= SIM_OVEN_MAX_DEAD_STEPS ||
        to_steps(c->probe_dead_s) >= SIM_OVEN_MAX_DEAD_STEPS || c->pwm_period_ms == 0) {
        return ROF_ERR_INVALID_ARG;
    }
    memset(sim, 0, sizeof(*sim));
    sim->config = *config;
    sim->chamber_dead_steps = to_steps(c->chamber_dead_s);
    sim->probe_dead_steps = to_steps(c->probe_dead_s);
    sim->chamber_c = c->ambient_c;
    sim->probe_c = c->ambient_c;
    for (size_t i = 0; i < SIM_OVEN_MAX_DEAD_STEPS; i++) {
        sim->chamber_history[i] = c->ambient_c;
    }
    return ROF_OK;
}

void sim_oven_advance(sim_oven_t *sim, uint64_t delta_ms)
{
    /* Auf ganze Schritte abgerundet */
    for (uint64_t done_ms = 0; done_ms + SIM_OVEN_STEP_MS <= delta_ms; done_ms += SIM_OVEN_STEP_MS) {
        step(sim);
    }
}

void sim_oven_freeze_chamber(sim_oven_t *sim)
{
    sim->chamber_frozen = true;
    sim->frozen_c = (float)sim->chamber_c;
}

rof_temp_sensor_t sim_oven_sensors(sim_oven_t *sim)
{
    rof_temp_sensor_t sensors = { .ctx = sim, .enumerate = sim_enumerate, .read = sim_read };
    return sensors;
}

rof_output_t sim_oven_output(sim_oven_t *sim)
{
    rof_output_t output = { .ctx = sim, .set_duty = sim_set_duty };
    return output;
}

rof_clock_t sim_oven_clock(sim_oven_t *sim)
{
    rof_clock_t clk = { .ctx = sim, .now_ms = sim_now_ms };
    return clk;
}
