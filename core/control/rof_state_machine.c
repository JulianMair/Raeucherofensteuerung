#include "rof_state_machine.h"

#include <stddef.h>

/* ---- Konfiguration ------------------------------------------------------ */

/* So formuliert, dass auch NaN die Konfiguration ungültig macht. */
static bool config_is_valid(const rof_fsm_config_t *c)
{
    return c->hold_band_c > 0.0f && c->reheat_band_c > c->hold_band_c && c->cooled_c > 0.0f;
}

/* ---- Zustandswechsel ---------------------------------------------------- */

static const char *const STATE_TEXT[] = {
    [ROF_STATE_IDLE] = "Zustand: BEREIT",
    [ROF_STATE_HEATING] = "Zustand: AUFHEIZEN",
    [ROF_STATE_HOLDING] = "Zustand: HALTEN",
    [ROF_STATE_COOLING] = "Zustand: ABKUEHLEN",
    [ROF_STATE_FAULT] = "Zustand: FEHLER",
    [ROF_STATE_SAFE_STATE] = "Zustand: SICHERER_ZUSTAND",
};

/* Jeder Zustandswechsel läuft hierüber und wird einmal geloggt. */
static void enter(rof_fsm_t *fsm, rof_state_t next)
{
    if (next != fsm->state) {
        fsm->state = next;
        rof_log_write(fsm->logger, ROF_LOG_INFO, STATE_TEXT[next]);
    }
}

/* Übergänge im Normalbetrieb, also ohne Fehler. In AUFHEIZEN und HALTEN
 * ist immer ein Sollwert gesetzt, denn ohne ihn gibt es keinen Start. */
static rof_state_t next_regular_state(const rof_fsm_t *fsm, float chamber_c)
{
    const rof_fsm_config_t *c = &fsm->config;
    switch (fsm->state) {
    case ROF_STATE_HEATING:
        if (chamber_c >= fsm->setpoint_c - c->hold_band_c) {
            return ROF_STATE_HOLDING; /* Ü2 */
        }
        return ROF_STATE_HEATING;
    case ROF_STATE_HOLDING:
        if (chamber_c < fsm->setpoint_c - c->reheat_band_c) {
            return ROF_STATE_HEATING; /* Ü3 */
        }
        return ROF_STATE_HOLDING;
    case ROF_STATE_COOLING:
        if (chamber_c < c->cooled_c) {
            return ROF_STATE_IDLE; /* Ü5 */
        }
        return ROF_STATE_COOLING;
    case ROF_STATE_FAULT:
        return ROF_STATE_IDLE; /* Ü8: keine Ursache mehr aktiv */
    case ROF_STATE_SAFE_STATE:
        return ROF_STATE_IDLE; /* Ü10: Reset gelungen, nie direkt zurück ins Heizen */
    default:
        return fsm->state;
    }
}

/* ---- Öffentliche Funktionen --------------------------------------------- */

rof_fsm_config_t rof_fsm_default_config(void)
{
    return (rof_fsm_config_t){
        .hold_band_c = 2.0f,
        .reheat_band_c = 10.0f,
        .cooled_c = 50.0f,
    };
}

rof_status_t rof_fsm_init(rof_fsm_t *fsm, const rof_fsm_config_t *config, const rof_log_t *logger)
{
    if (fsm == NULL || config == NULL || logger == NULL || logger->write == NULL) {
        return ROF_ERR_INVALID_ARG;
    }
    if (!config_is_valid(config)) {
        return ROF_ERR_INVALID_ARG;
    }
    /* SAF-10: immer BEREIT, ohne Sollwert. Geheizt wird erst nach Start. */
    *fsm = (rof_fsm_t){
        .config = *config,
        .logger = logger,
        .state = ROF_STATE_IDLE,
        .has_setpoint = false,
    };
    return ROF_OK;
}

rof_state_t rof_fsm_state(const rof_fsm_t *fsm)
{
    return fsm->state;
}

bool rof_fsm_output_enabled(const rof_fsm_t *fsm)
{
    return fsm->state == ROF_STATE_HEATING || fsm->state == ROF_STATE_HOLDING;
}

bool rof_fsm_roles_editable(const rof_fsm_t *fsm)
{
    return !rof_fsm_output_enabled(fsm);
}

bool rof_fsm_setpoint(const rof_fsm_t *fsm, float *setpoint_c)
{
    if (!fsm->has_setpoint) {
        return false;
    }
    *setpoint_c = fsm->setpoint_c;
    return true;
}

rof_cmd_result_t rof_fsm_set_setpoint(rof_fsm_t *fsm, const rof_safety_t *safety, float setpoint_c)
{
    switch (rof_safety_check_setpoint(safety, setpoint_c)) {
    case ROF_SETPOINT_OK:
        fsm->setpoint_c = setpoint_c;
        fsm->has_setpoint = true;
        return ROF_CMD_OK;
    case ROF_SETPOINT_TOO_LOW:
        return ROF_CMD_SETPOINT_TOO_LOW;
    case ROF_SETPOINT_TOO_HIGH:
        return ROF_CMD_SETPOINT_TOO_HIGH;
    case ROF_SETPOINT_NOT_A_NUMBER:
    default:
        return ROF_CMD_SETPOINT_NOT_A_NUMBER;
    }
}

rof_cmd_result_t rof_fsm_start(rof_fsm_t *fsm)
{
    switch (fsm->state) {
    case ROF_STATE_IDLE:
    case ROF_STATE_COOLING:
        if (!fsm->has_setpoint) {
            return ROF_CMD_NO_SETPOINT;
        }
        enter(fsm, ROF_STATE_HEATING); /* Ü1, Ü6 */
        return ROF_CMD_OK;
    case ROF_STATE_HEATING:
    case ROF_STATE_HOLDING:
        return ROF_CMD_ALREADY_RUNNING;
    case ROF_STATE_FAULT:
        return ROF_CMD_FAULT_ACTIVE;
    case ROF_STATE_SAFE_STATE:
    default:
        return ROF_CMD_SAFE_STATE;
    }
}

rof_cmd_result_t rof_fsm_stop(rof_fsm_t *fsm)
{
    if (rof_fsm_output_enabled(fsm)) {
        enter(fsm, ROF_STATE_COOLING); /* Ü4 */
    }
    return ROF_CMD_OK;
}

/* Reihenfolge: verriegelt vor aktiv vor Normalbetrieb. So gewinnt immer
 * der strengere Zustand. */
void rof_fsm_step(rof_fsm_t *fsm, const rof_fsm_input_t *input)
{
    if (input->latched != ROF_FAULT_NONE) {
        /* Ü9: aus jedem Zustand. Raus nur über einen gelungenen Reset (Ü10). */
        enter(fsm, ROF_STATE_SAFE_STATE);
        return;
    }
    if (input->active != ROF_FAULT_NONE) {
        /* Ü7: Ursache aktiv, aber nicht verriegelt. Ohne gültigen Garraumwert
         * gibt es keinen Normalbetrieb, also FEHLER mit Ausgang aus. */
        enter(fsm, ROF_STATE_FAULT);
        return;
    }
    enter(fsm, next_regular_state(fsm, input->chamber_temp_c));
}
