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

/* ---- Befehle (ARC-05, ARC-06) ------------------------------------------- */

static void record_result(rof_controller_t *ctrl, const rof_command_t *command,
                          rof_cmd_result_t result)
{
    ctrl->recent[ctrl->recent_next] = (rof_command_record_t){
        .seq = command->seq,
        .type = command->type,
        .result = result,
    };
    ctrl->recent_next = (ctrl->recent_next + 1) % ROF_CONTROLLER_RECENT_RESULTS;
    if (ctrl->recent_count < ROF_CONTROLLER_RECENT_RESULTS) {
        ctrl->recent_count++;
    }
    if (result != ROF_CMD_OK) {
        rof_log_write(ctrl->io.logger, ROF_LOG_WARN, "Befehl abgelehnt");
    }
}

/* Antwort der Sensorverwaltung als Befehlsergebnis */
static rof_cmd_result_t from_registry(rof_status_t status)
{
    switch (status) {
    case ROF_OK:
        return ROF_CMD_OK;
    case ROF_ERR_NOT_FOUND:
        return ROF_CMD_UNKNOWN_SENSOR;
    case ROF_ERR_INVALID_ARG:
        return ROF_CMD_INVALID_VALUE;
    default:
        return ROF_CMD_STORAGE_ERROR;
    }
}

/* SAF-02: Reset nur im sicheren Zustand und nur ohne bestehende Ursache.
 * Geprüft wird mit dem Garraumwert aus dem letzten Zyklus. Gelingt er, sieht
 * der Automat noch im selben Zyklus "nicht mehr verriegelt" (Ü10). */
static rof_cmd_result_t reset_safe_state(rof_controller_t *ctrl)
{
    if (rof_fsm_state(&ctrl->fsm) != ROF_STATE_SAFE_STATE) {
        return ROF_CMD_NOT_IN_SAFE_STATE;
    }
    float chamber_c = 0.0f;
    const rof_safety_input_t input = {
        .chamber_status = rof_sensor_registry_role(&ctrl->registry, ROF_ROLE_CHAMBER, &chamber_c),
        .chamber_temp_c = chamber_c,
        .applied_duty = ctrl->applied_duty,
        .heating = false,
    };
    if (rof_safety_reset(&ctrl->safety, &input) != ROF_FAULT_NONE) {
        return ROF_CMD_CAUSE_ACTIVE;
    }
    return ROF_CMD_OK;
}

/* ARC-06: Der Regeltask prüft jeden Befehl selbst. Unbekanntes wird verworfen. */
static rof_cmd_result_t dispatch(rof_controller_t *ctrl, const rof_command_t *command)
{
    switch (command->type) {
    case ROF_COMMAND_SET_SETPOINT:
        return rof_fsm_set_setpoint(&ctrl->fsm, &ctrl->safety, command->value_c);
    case ROF_COMMAND_START:
        return rof_fsm_start(&ctrl->fsm);
    case ROF_COMMAND_STOP:
        return rof_fsm_stop(&ctrl->fsm);
    case ROF_COMMAND_ASSIGN_ROLE:
        if (!rof_fsm_roles_editable(&ctrl->fsm)) {
            return ROF_CMD_SENSORS_LOCKED; /* E-022 */
        }
        return from_registry(rof_sensor_registry_assign(&ctrl->registry, command->sensor_id, command->role));
    case ROF_COMMAND_SET_OFFSET:
        if (!rof_fsm_roles_editable(&ctrl->fsm)) {
            return ROF_CMD_SENSORS_LOCKED; /* E-026: ein Offset verschiebt den Messwert wie ein Fühlerwechsel */
        }
        return from_registry(rof_sensor_registry_set_offset(&ctrl->registry, command->sensor_id, command->value_c));
    case ROF_COMMAND_RESET:
        return reset_safe_state(ctrl);
    default:
        return ROF_CMD_UNKNOWN_COMMAND;
    }
}

/* Schritt 1: höchstens max_commands_per_cycle Befehle, der Rest wartet auf
 * den nächsten Zyklus. Eine geflutete Queue verzögert so nie die Regelung (ARC-09). */
static void process_commands(rof_controller_t *ctrl)
{
    for (unsigned i = 0; i < ctrl->config.max_commands_per_cycle; i++) {
        rof_command_t command;
        if (!rof_command_source_receive(ctrl->io.commands, &command)) {
            return;
        }
        record_result(ctrl, &command, dispatch(ctrl, &command));
    }
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
    ctrl->recent_count = 0;
    ctrl->recent_next = 0;
    /* SAF-10: nach jedem Start ausdrücklich aus, egal was vorher war */
    apply_duty(ctrl, 0.0f);
    return ROF_OK;
}

void rof_controller_snapshot(const rof_controller_t *ctrl, rof_snapshot_t *snapshot)
{
    const rof_sensor_registry_t *reg = &ctrl->registry;
    *snapshot = (rof_snapshot_t){
        .time_ms = ctrl->last_cycle_ms,
        .state = rof_fsm_state(&ctrl->fsm),
        .duty = ctrl->applied_duty,
        .latched_fault = rof_safety_fault(&ctrl->safety),
        .active_fault = rof_safety_active(&ctrl->safety),
        .sensor_overflow = rof_sensor_registry_overflow(reg),
    };
    snapshot->has_setpoint = rof_fsm_setpoint(&ctrl->fsm, &snapshot->setpoint_c);

    snapshot->roles[ROF_ROLE_UNASSIGNED].status = ROF_ROLE_STATUS_UNASSIGNED;
    for (int role = ROF_ROLE_UNASSIGNED + 1; role < ROF_ROLE_COUNT; role++) {
        rof_role_snapshot_t *r = &snapshot->roles[role];
        r->status = rof_sensor_registry_role(reg, (rof_role_t)role, &r->temp_c);
    }

    snapshot->channel_count = rof_sensor_registry_channel_count(reg);
    for (size_t i = 0; i < snapshot->channel_count; i++) {
        const rof_sensor_channel_t *ch = rof_sensor_registry_channel(reg, i);
        snapshot->channels[i] = (rof_channel_snapshot_t){
            .id = ch->id,
            .type = ch->type,
            .role = ch->role,
            .offset_c = ch->offset_c,
            .present = ch->present,
            .duplicate = ch->duplicate,
            .read_status = ch->read_status,
            .temp_c = ch->temp_c,
        };
    }

    /* Ringpuffer neuestes zuerst: rückwärts ab dem zuletzt geschriebenen */
    snapshot->recent_count = ctrl->recent_count;
    for (size_t i = 0; i < ctrl->recent_count; i++) {
        const size_t index = (ctrl->recent_next + ROF_CONTROLLER_RECENT_RESULTS - 1 - i) %
                             ROF_CONTROLLER_RECENT_RESULTS;
        snapshot->recent[i] = ctrl->recent[index];
    }
}

bool rof_snapshot_command_result(const rof_snapshot_t *snapshot, uint32_t seq,
                                 rof_cmd_result_t *result)
{
    for (size_t i = 0; i < snapshot->recent_count; i++) {
        if (snapshot->recent[i].seq == seq) {
            *result = snapshot->recent[i].result;
            return true;
        }
    }
    return false;
}

void rof_controller_cycle(rof_controller_t *ctrl)
{
    const uint64_t now_ms = rof_clock_now_ms(ctrl->io.clk);
    const float dt_s = seconds_since_last_cycle(ctrl, now_ms);

    /* 1. Befehle */
    process_commands(ctrl);

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
