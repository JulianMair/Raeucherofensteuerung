/*
 * Fertigzeit-Prognose (FC-01, E-047).
 *
 * Schätzt, wann ein Kernfühler das Kernziel erreicht. Modell: Der Kern
 * nähert sich dem Garraum wie ein System erster Ordnung,
 *     dT_k/dt = (T_g - T_k) / tau.
 * Dann fällt ln(T_g - T_k) über die Zeit linear mit der Steigung -1/tau.
 * Eine lineare Regression über die letzten Punkte liefert tau, und die
 * Restzeit bis zum Ziel ist
 *     tau * ln((T_g - T_k) / (T_g - T_ziel)).
 *
 * Nur Anzeige: Die Prognose wirkt nie auf Regelung oder Sicherheitslogik.
 */
#ifndef ROF_FORECAST_H
#define ROF_FORECAST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ROF_FORECAST_SAMPLE_MS 10000u /* höchstens ein Punkt je 10 s */
#define ROF_FORECAST_WINDOW 60u       /* die letzten 10 Minuten */
#define ROF_FORECAST_MIN_SAMPLES 30u  /* 5 Minuten Lernzeit */
#define ROF_FORECAST_MAX_REMAINING_S 86400u /* mehr als 24 h gilt als nicht erreichbar */
#define ROF_FORECAST_MIN_GAP_C 0.5f   /* so weit muss das Ziel unter dem Garraum liegen */

/* In dieser Reihenfolge geprüft */
typedef enum {
    ROF_FORECAST_UNAVAILABLE = 0, /* Kern- oder Garraumfühler ohne gültigen Wert */
    ROF_FORECAST_NO_TARGET,       /* kein Kernziel gesetzt */
    ROF_FORECAST_REACHED,         /* Kern hat das Ziel erreicht */
    ROF_FORECAST_WAITING,         /* nicht in HALTEN, der Garraum steigt noch */
    ROF_FORECAST_LEARNING,        /* noch weniger als ROF_FORECAST_MIN_SAMPLES Punkte */
    ROF_FORECAST_OK,              /* remaining_s ist gültig */
    ROF_FORECAST_UNREACHABLE      /* Ziel nicht unter dem Garraum, keine Annäherung oder über 24 h */
} rof_forecast_status_t;

typedef struct {
    rof_forecast_status_t status;
    uint32_t remaining_s; /* nur bei ROF_FORECAST_OK, sonst 0 */
} rof_forecast_result_t;

/* Was der Regelzyklus je Kernfühler übergibt. Die Prognose kennt die
 * Sensorverwaltung nicht, nur ob ein Wert gültig ist. */
typedef struct {
    bool holding;    /* Automat in HALTEN: Der Garraum hält seinen Sollwert */
    bool chamber_ok;
    float chamber_c;
    bool probe_ok;
    float probe_c;
    bool has_target;
    float target_c;
} rof_forecast_input_t;

typedef struct {
    uint64_t time_ms[ROF_FORECAST_WINDOW];
    float chamber_c[ROF_FORECAST_WINDOW];
    float probe_c[ROF_FORECAST_WINDOW];
    size_t count; /* gültige Punkte, höchstens ROF_FORECAST_WINDOW */
    size_t next;  /* hier kommt der nächste Punkt hin (Ringpuffer) */
} rof_forecast_t;

void rof_forecast_init(rof_forecast_t *fc);

/* Vergisst alle Punkte, das Lernen beginnt von vorn. */
void rof_forecast_clear(rof_forecast_t *fc);

/* Nimmt einen Punkt auf, wenn seit dem letzten mindestens
 * ROF_FORECAST_SAMPLE_MS vergangen sind. true, wenn er aufgenommen wurde.
 * Ist das Fenster voll, fällt der älteste Punkt heraus. */
bool rof_forecast_add(rof_forecast_t *fc, uint64_t now_ms, float chamber_c, float probe_c);

/* Zeitkonstante tau in Sekunden aus den gesammelten Punkten, gerechnet mit
 * dem Mittelwert des Garraums. false, und *tau_s bleibt unverändert, bei
 * weniger als zwei Punkten, wenn ein Kern nicht unter dem Garraum lag oder
 * wenn er sich nicht annähert. */
bool rof_forecast_time_constant(const rof_forecast_t *fc, float *tau_s);

/* Einmal je Regelzyklus. Ohne gültige Werte oder außerhalb von HALTEN
 * beginnt das Lernen von vorn; sonst kommt höchstens alle
 * ROF_FORECAST_SAMPLE_MS ein Punkt dazu. Ohne Ziel wird trotzdem gelernt. */
rof_forecast_result_t rof_forecast_update(rof_forecast_t *fc, const rof_forecast_input_t *in, uint64_t now_ms);

#endif /* ROF_FORECAST_H */
