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
#include "rof_safety.h"
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

/* Ergebnis eines Befehls. Bei Ablehnung bleibt der Zustand, und der Grund
 * wird gemeldet (SAF-09, E-022). */
typedef enum {
    ROF_CMD_OK = 0,
    ROF_CMD_NO_SETPOINT,          /* Start ohne gesetzten Sollwert */
    ROF_CMD_ALREADY_RUNNING,      /* Start während AUFHEIZEN oder HALTEN */
    ROF_CMD_FAULT_ACTIVE,         /* Start in FEHLER */
    ROF_CMD_SAFE_STATE,           /* Start in SICHERER_ZUSTAND */
    ROF_CMD_SETPOINT_TOO_LOW,     /* Sollwert unter der Grenze */
    ROF_CMD_SETPOINT_TOO_HIGH,    /* Sollwert über der Grenze */
    ROF_CMD_SETPOINT_NOT_A_NUMBER /* Sollwert ist keine Zahl */
} rof_cmd_result_t;

/* Was der Regelzyklus dem Automaten je Zyklus übergibt. */
typedef struct {
    rof_fault_t latched;  /* rof_safety_fault(): verriegelte Ursache */
    rof_fault_t active;   /* rof_safety_active(): Ursache in diesem Zyklus */
    float chamber_temp_c; /* Garraumwert, nur gültig bei active == ROF_FAULT_NONE */
} rof_fsm_input_t;

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

/* Befehl Sollwert setzen, in jedem Zustand erlaubt. Geprüft wird mit
 * rof_safety_check_setpoint(); ein ungültiger Wert wird abgelehnt, nicht
 * begrenzt, und der alte Sollwert bleibt (SAF-09). */
rof_cmd_result_t rof_fsm_set_setpoint(rof_fsm_t *fsm, const rof_safety_t *safety, float setpoint_c);

/* Befehl Start. Aus BEREIT oder ABKUEHLEN nach AUFHEIZEN, wenn ein
 * Sollwert gesetzt ist. Sonst abgelehnt mit Grund. */
rof_cmd_result_t rof_fsm_start(rof_fsm_t *fsm);

/* Befehl Stopp. Aus AUFHEIZEN oder HALTEN nach ABKUEHLEN. In allen
 * anderen Zuständen ist der Ausgang schon aus; dann tut Stopp nichts und
 * gilt trotzdem als erfolgreich. */
rof_cmd_result_t rof_fsm_stop(rof_fsm_t *fsm);

/* Ein Regelzyklus: wechselt den Zustand nach der Übergangstabelle in
 * docs/zustandsautomat.md. Aufrufen nach der Sicherheitslogik und vor dem Regler. */
void rof_fsm_step(rof_fsm_t *fsm, const rof_fsm_input_t *input);

#endif /* ROF_STATE_MACHINE_H */
