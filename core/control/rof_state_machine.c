#include "rof_state_machine.h"

#include <stddef.h>

/* ---- Konfiguration ------------------------------------------------------ */

/* So formuliert, dass auch NaN die Konfiguration ungültig macht. */
static bool config_is_valid(const rof_fsm_config_t *c)
{
    return c->hold_band_c > 0.0f && c->reheat_band_c > c->hold_band_c && c->cooled_c > 0.0f;
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
