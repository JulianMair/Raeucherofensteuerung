/*
 * Sicherheitslogik (SAF-01 bis SAF-12).
 *
 * Wird in jedem Regelzyklus vor dem Regler ausgewertet (SAF-01). Löst
 * eine Regel aus, verriegelt das Modul mit dieser Ursache: Der Ausgang
 * ist 0, bis ein Reset gelingt, und ein Reset gelingt nur, wenn keine
 * Regel mehr auslöst (SAF-02).
 *
 * Die Sicherheitslogik kennt die Sensorverwaltung nicht direkt. Der
 * Regelzyklus übergibt ihr je Zyklus ein rof_safety_input_t.
 */
#ifndef ROF_SAFETY_H
#define ROF_SAFETY_H

#include "rof_clock.h"
#include "rof_log.h"
#include "rof_status.h"

#include <stdint.h>

/* Harte Obergrenze für den Garraum in °C (SAF-06). Bewusst kein Teil
 * der Konfiguration und damit nicht über die API änderbar, nur beim
 * Bauen, z. B. später für einen PT1000. Sie liegt unter dem Messbereichsende
 * des DS18B20 (125 °C), sonst würde sie nie auslösen. */
#ifndef ROF_SAFETY_HARD_MAX_C
#define ROF_SAFETY_HARD_MAX_C 120.0f
#endif

typedef struct {
    float plausible_min_c;       /* SAF-04: Garraumwerte außerhalb sind unplausibel */
    float plausible_max_c;
    float setpoint_min_c;        /* SAF-09: erlaubter Bereich für Sollwerte */
    float setpoint_max_c;
    uint32_t freeze_timeout_ms;  /* SAF-05: so lange darf sich der Wert bei aktivem Ausgang nicht ändern */
    float freeze_min_change_c;   /* SAF-05: kleinere Änderungen zählen nicht als Änderung */
    uint32_t cycle_window_ms;    /* SAF-08: größter erlaubter Abstand zweier Regelzyklen */
} rof_safety_config_t;

typedef struct {
    rof_safety_config_t config;
    const rof_clock_t *clk;
    const rof_log_t *logger;
} rof_safety_t;

/* Ergebnis der Sollwertprüfung (SAF-09). Abgelehnt wird mit Grund, nie begrenzt. */
typedef enum {
    ROF_SETPOINT_OK = 0,
    ROF_SETPOINT_TOO_LOW,
    ROF_SETPOINT_TOO_HIGH,
    ROF_SETPOINT_NOT_A_NUMBER
} rof_setpoint_check_t;

/* Die vereinbarten Standardwerte (E-016). */
rof_safety_config_t rof_safety_default_config(void);

/* Prüft Interfaces und Konfiguration und setzt das Modul auf.
 * Die Konfiguration wird kopiert, Uhr und Log müssen gültig bleiben.
 * ROF_ERR_INVALID_ARG: ein Interface fehlt oder die Konfiguration ist
 * nicht stimmig. Stimmig heißt: Minimum unter Maximum, Sollwertbereich
 * innerhalb des plausiblen Bereichs und echt unter ROF_SAFETY_HARD_MAX_C,
 * alle Zeiten und die Mindeständerung größer als 0. */
rof_status_t rof_safety_init(rof_safety_t *safety, const rof_safety_config_t *config,
                             const rof_clock_t *clk, const rof_log_t *logger);

rof_setpoint_check_t rof_safety_check_setpoint(const rof_safety_t *safety, float setpoint_c);

#endif /* ROF_SAFETY_H */
