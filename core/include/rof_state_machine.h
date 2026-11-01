/*
 * Zustandsautomat (CTL-04, CTL-05). Übergangstabelle: docs/zustandsautomat.md
 *
 * Der Automat ruft weder Sicherheitslogik noch Sensorverwaltung selbst auf.
 * Der Regelzyklus übergibt ihm je Zyklus deren Ergebnis (rof_fsm_input_t)
 * und reicht Befehle an ihn weiter.
 */
#ifndef ROF_STATE_MACHINE_H
#define ROF_STATE_MACHINE_H

#include "rof_log.h"
#include "rof_status.h"

#include <stdbool.h>

/* Die deutschen Namen stehen in docs/glossar.md. */
typedef enum {
    ROF_STATE_IDLE = 0,  /* BEREIT */
    ROF_STATE_HEATING,   /* AUFHEIZEN */
    ROF_STATE_HOLDING,   /* HALTEN */
    ROF_STATE_COOLING,   /* ABKUEHLEN */
    ROF_STATE_FAULT,     /* FEHLER */
    ROF_STATE_SAFE_STATE /* SICHERER_ZUSTAND */
} rof_state_t;

typedef struct {
    float hold_band_c;   /* AUFHEIZEN -> HALTEN ab Sollwert - hold_band_c */
    float reheat_band_c; /* HALTEN -> AUFHEIZEN unter Sollwert - reheat_band_c */
    float cooled_c;      /* ABKUEHLEN -> BEREIT unter cooled_c */
} rof_fsm_config_t;

typedef struct {
    rof_fsm_config_t config;
    const rof_log_t *logger;
    rof_state_t state;
    bool has_setpoint;
    float setpoint_c;
} rof_fsm_t;

/* Die vereinbarten Standardwerte (E-021). */
rof_fsm_config_t rof_fsm_default_config(void);

/* Setzt den Automaten auf: Zustand BEREIT, kein Sollwert (SAF-10).
 * ROF_ERR_INVALID_ARG: ein Zeiger fehlt oder die Konfiguration ist nicht
 * stimmig. Stimmig heißt: hold_band_c > 0, reheat_band_c > hold_band_c
 * (sonst spränge der Zustand ständig hin und her) und cooled_c > 0. */
rof_status_t rof_fsm_init(rof_fsm_t *fsm, const rof_fsm_config_t *config, const rof_log_t *logger);

rof_state_t rof_fsm_state(const rof_fsm_t *fsm);

/* true nur in AUFHEIZEN und HALTEN: Nur dann darf der Regler heizen. */
bool rof_fsm_output_enabled(const rof_fsm_t *fsm);

/* false in AUFHEIZEN und HALTEN: Rollen zuweisen ist dann gesperrt (E-022). */
bool rof_fsm_roles_editable(const rof_fsm_t *fsm);

/* Schreibt den Sollwert nach *setpoint_c und liefert true. Ohne gesetzten
 * Sollwert false, *setpoint_c bleibt unverändert. */
bool rof_fsm_setpoint(const rof_fsm_t *fsm, float *setpoint_c);

#endif /* ROF_STATE_MACHINE_H */
