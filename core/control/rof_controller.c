#include "rof_controller.h"

/* ---- Hilfsfunktionen ---------------------------------------------------- */

static bool io_is_valid(const rof_controller_io_t *io)
{
    return io->output != NULL && io->output->set_duty != NULL && io->commands != NULL &&
           io->commands->receive != NULL;
}

/* Sekunden seit dem letzten Zyklus, für den I-Anteil. 0 im ersten Zyklus. */
static float seconds_since_last_cycle(rof_controller_t *ctrl, uint64_t now_ms)
{
    float dt_s = 0.0f;
    if (ctrl->has_last_cycle) {
        dt_s = (float)(now_ms - ctrl->last_cycle_ms) / 1000.0f;
    }
    ctrl->has_last_cycle = true;
    ctrl->last_cycle_ms = now_ms;
    return dt_s;
}

/* Schritt 5: PI-Regler nur, wenn der Automat den Ausgang freigibt. Beim
 * Eintritt ins Heizen beginnt der Regler mit leerem I-Anteil. */
static float requested_duty(rof_controller_t *ctrl, float chamber_c, float dt_s)
{
    const bool heating = rof_fsm_output_enabled(&ctrl->fsm);
    if (heating && !ctrl->was_heating) {
        rof_pi_reset(&ctrl->pi);
    }
    ctrl->was_heating = heating;
    if (!heating) {
        return 0.0f;
    }
    float setpoint_c = 0.0f;
    rof_fsm_setpoint(&ctrl->fsm, &setpoint_c); /* beim Heizen immer gesetzt (E-022) */
    return rof_pi_update(&ctrl->pi, setpoint_c, chamber_c, dt_s);
}

static void apply_duty(rof_controller_t *ctrl, float duty)
{
    if (rof_output_set_duty(ctrl->io.output, duty) != ROF_OK) {
        rof_log_write(ctrl->io.logger, ROF_LOG_ERROR, "Ausgang lässt sich nicht setzen");
    }
    ctrl->applied_duty = duty;
}

/* ---- Öffentliche Funktionen --------------------------------------------- */

rof_controller_config_t rof_controller_default_config(void)
{
    return (rof_controller_config_t){
        .max_commands_per_cycle = ROF_CONTROLLER_DEFAULT_MAX_COMMANDS,
        .safety = rof_safety_default_config(),
        .fsm = rof_fsm_default_config(),
        .pi = rof_pi_default_config(),
    };
}

rof_status_t rof_controller_init(rof_controller_t *ctrl, const rof_controller_config_t *config,
                                 const rof_controller_io_t *io)
{
    if (ctrl == NULL || config == NULL || io == NULL || !io_is_valid(io) ||
        config->max_commands_per_cycle == 0) {
        return ROF_ERR_INVALID_ARG;
    }
    ctrl->config = *config;
    ctrl->io = *io;
    /* Jedes Modul prüft seine eigenen Interfaces und seine Konfiguration. */
    rof_status_t status = rof_sensor_registry_init(&ctrl->registry, io->drivers, io->driver_count,
                                                   io->storage, io->logger);
    if (status == ROF_OK) {
        status = rof_safety_init(&ctrl->safety, &config->safety, io->clk, io->logger);
    }
    if (status == ROF_OK) {
        status = rof_fsm_init(&ctrl->fsm, &config->fsm, io->logger);
    }
    if (status == ROF_OK) {
        status = rof_pi_init(&ctrl->pi, &config->pi);
    }
    if (status != ROF_OK) {
        return status;
    }
    ctrl->was_heating = false;
    ctrl->has_last_cycle = false;
    /* SAF-10: nach jedem Start ausdrücklich aus, egal was vorher war */
    apply_duty(ctrl, 0.0f);
    return ROF_OK;
}

void rof_controller_cycle(rof_controller_t *ctrl)
{
    const uint64_t now_ms = rof_clock_now_ms(ctrl->io.clk);
    const float dt_s = seconds_since_last_cycle(ctrl, now_ms);

    /* 2. Sensoren lesen */
    rof_sensor_registry_update(&ctrl->registry);
    float chamber_c = 0.0f;
    const rof_role_status_t chamber_status =
        rof_sensor_registry_role(&ctrl->registry, ROF_ROLE_CHAMBER, &chamber_c);

    /* 3. Sicherheitslogik, vor dem Regler (SAF-01) */
    const rof_safety_input_t safety_input = {
        .chamber_status = chamber_status,
        .chamber_temp_c = chamber_c,
        .applied_duty = ctrl->applied_duty,
        .heating = rof_fsm_output_enabled(&ctrl->fsm),
    };
    rof_safety_evaluate(&ctrl->safety, &safety_input);

    /* 4. Zustandsautomat */
    const rof_fsm_input_t fsm_input = {
        .latched = rof_safety_fault(&ctrl->safety),
        .active = rof_safety_active(&ctrl->safety),
        .chamber_temp_c = chamber_c,
    };
    rof_fsm_step(&ctrl->fsm, &fsm_input);

    /* 5. bis 7.: Regler, letztes Wort der Sicherheitslogik (SAF-02), Ausgang */
    const float duty = requested_duty(ctrl, chamber_c, dt_s);
    apply_duty(ctrl, rof_safety_limit_duty(&ctrl->safety, duty));
}
