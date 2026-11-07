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

#endif /* ROF_FORECAST_H */
