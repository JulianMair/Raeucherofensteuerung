#include "rof_proto.h"

#include <string.h>

/* Namen in der API, genau wie die enum-Listen in docs/api.yaml (E-028).
 * Jede Tabelle ist nach dem Wert des Enums geordnet. */

static const char *const STATE_NAMES[] = {
    [ROF_STATE_IDLE] = "idle",       [ROF_STATE_HEATING] = "heating",
    [ROF_STATE_HOLDING] = "holding", [ROF_STATE_COOLING] = "cooling",
    [ROF_STATE_FAULT] = "fault",     [ROF_STATE_SAFE_STATE] = "safe_state",
};

static const char *const ROLE_NAMES[] = {
    [ROF_ROLE_UNASSIGNED] = "unassigned", [ROF_ROLE_CHAMBER] = "chamber",
    [ROF_ROLE_PROBE_1] = "probe_1",       [ROF_ROLE_PROBE_2] = "probe_2",
    [ROF_ROLE_SMOKE_GEN] = "smoke_gen",
};

static const char *const ROLE_STATUS_NAMES[] = {
    [ROF_ROLE_STATUS_OK] = "ok",
    [ROF_ROLE_STATUS_UNASSIGNED] = "unassigned",
    [ROF_ROLE_STATUS_MISSING] = "missing",
    [ROF_ROLE_STATUS_READ_ERROR] = "read_error",
    [ROF_ROLE_STATUS_DUPLICATE] = "duplicate",
};

static const char *const FAULT_NAMES[] = {
    [ROF_FAULT_NONE] = "none",
    [ROF_FAULT_CHAMBER_UNASSIGNED] = "chamber_unassigned",
    [ROF_FAULT_CHAMBER_MISSING] = "chamber_missing",
    [ROF_FAULT_CHAMBER_READ_ERROR] = "chamber_read_error",
    [ROF_FAULT_CHAMBER_DUPLICATE] = "chamber_duplicate",
    [ROF_FAULT_IMPLAUSIBLE] = "implausible",
    [ROF_FAULT_OVER_TEMPERATURE] = "over_temperature",
    [ROF_FAULT_FROZEN] = "frozen",
    [ROF_FAULT_CYCLE_OVERRUN] = "cycle_overrun",
};

/* Der Befehlstyp beginnt bei 1, Platz 0 bleibt leer. */
static const char *const COMMAND_NAMES[] = {
    [ROF_COMMAND_SET_SETPOINT] = "set_setpoint", [ROF_COMMAND_START] = "start",
    [ROF_COMMAND_STOP] = "stop",                 [ROF_COMMAND_ASSIGN_ROLE] = "assign_role",
    [ROF_COMMAND_SET_OFFSET] = "set_offset",     [ROF_COMMAND_RESET] = "reset",
    [ROF_COMMAND_SET_CORE_TARGET] = "set_core_target",
};

static const char *const RESULT_NAMES[] = {
    [ROF_CMD_OK] = "ok",
    [ROF_CMD_NO_SETPOINT] = "no_setpoint",
    [ROF_CMD_ALREADY_RUNNING] = "already_running",
    [ROF_CMD_FAULT_ACTIVE] = "fault_active",
    [ROF_CMD_SAFE_STATE] = "safe_state",
    [ROF_CMD_SETPOINT_TOO_LOW] = "setpoint_too_low",
    [ROF_CMD_SETPOINT_TOO_HIGH] = "setpoint_too_high",
    [ROF_CMD_SETPOINT_NOT_A_NUMBER] = "setpoint_not_a_number",
    [ROF_CMD_SENSORS_LOCKED] = "sensors_locked",
    [ROF_CMD_UNKNOWN_SENSOR] = "unknown_sensor",
    [ROF_CMD_INVALID_VALUE] = "invalid_value",
    [ROF_CMD_STORAGE_ERROR] = "storage_error",
    [ROF_CMD_NOT_IN_SAFE_STATE] = "not_in_safe_state",
    [ROF_CMD_CAUSE_ACTIVE] = "cause_active",
    [ROF_CMD_UNKNOWN_COMMAND] = "unknown_command",
    [ROF_CMD_CORE_TARGET_TOO_LOW] = "core_target_too_low",
    [ROF_CMD_CORE_TARGET_TOO_HIGH] = "core_target_too_high",
};

static const char *const FORECAST_NAMES[] = {
    [ROF_FORECAST_UNAVAILABLE] = "unavailable", [ROF_FORECAST_NO_TARGET] = "no_target",
    [ROF_FORECAST_REACHED] = "reached",         [ROF_FORECAST_WAITING] = "waiting",
    [ROF_FORECAST_LEARNING] = "learning",       [ROF_FORECAST_OK] = "ok",
    [ROF_FORECAST_UNREACHABLE] = "unreachable",
};

#define COUNT(table) (sizeof(table) / sizeof((table)[0]))

/* Name aus der Tabelle, "unknown" für Werte außerhalb oder leere Plätze */
static const char *lookup(const char *const *table, size_t count, int value)
{
    if (value < 0 || (size_t)value >= count || table[value] == NULL) {
        return "unknown";
    }
    return table[value];
}

const char *rof_proto_state_name(rof_state_t state)
{
    return lookup(STATE_NAMES, COUNT(STATE_NAMES), (int)state);
}

const char *rof_proto_role_name(rof_role_t role)
{
    return lookup(ROLE_NAMES, COUNT(ROLE_NAMES), (int)role);
}

bool rof_proto_role_from_name(const char *name, rof_role_t *role)
{
    for (size_t i = 0; i < COUNT(ROLE_NAMES); i++) {
        if (strcmp(ROLE_NAMES[i], name) == 0) {
            *role = (rof_role_t)i;
            return true;
        }
    }
    return false;
}

const char *rof_proto_role_status_name(rof_role_status_t status)
{
    return lookup(ROLE_STATUS_NAMES, COUNT(ROLE_STATUS_NAMES), (int)status);
}

const char *rof_proto_fault_name(rof_fault_t fault)
{
    return lookup(FAULT_NAMES, COUNT(FAULT_NAMES), (int)fault);
}

/* Für Kanäle nur drei Fälle: gelesen, nicht da, sonst ein Lesefehler */
const char *rof_proto_read_status_name(rof_status_t status)
{
    if (status == ROF_OK) {
        return "ok";
    }
    if (status == ROF_ERR_NOT_FOUND) {
        return "not_found";
    }
    return "read_error";
}

const char *rof_proto_command_name(rof_command_type_t type)
{
    return lookup(COMMAND_NAMES, COUNT(COMMAND_NAMES), (int)type);
}

const char *rof_proto_result_name(rof_cmd_result_t result)
{
    return lookup(RESULT_NAMES, COUNT(RESULT_NAMES), (int)result);
}

const char *rof_proto_forecast_status_name(rof_forecast_status_t status)
{
    return lookup(FORECAST_NAMES, COUNT(FORECAST_NAMES), (int)status);
}
