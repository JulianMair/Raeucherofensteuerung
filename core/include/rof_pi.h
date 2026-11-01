/*
 * PI-Regler mit Anti-Windup (CTL-02, E-024).
 *
 * Tastgrad = P-Anteil + I-Anteil, begrenzt auf 0 bis 1.
 *   P-Anteil: kp * e, mit e = Sollwert - Messwert
 *   I-Anteil: wächst je Zeitschritt um kp * e * dt / ti
 * Anti-Windup: Steht der Ausgang am Anschlag und drückt der Fehler weiter
 * in dieselbe Richtung, wächst der I-Anteil nicht weiter. Sonst würde er
 * beim langen Aufheizen riesig und ließe den Ofen danach weit überschwingen.
 */
#ifndef ROF_PI_H
#define ROF_PI_H

#include "rof_status.h"

typedef struct {
    float kp;   /* Verstärkung: Tastgrad je °C Regelabweichung */
    float ti_s; /* Nachstellzeit in Sekunden */
} rof_pi_config_t;

typedef struct {
    rof_pi_config_t config;
    float integral; /* I-Anteil als Tastgrad */
} rof_pi_t;

/* Standardwerte nach der SIMC-Regel für den Simulator (E-024). */
rof_pi_config_t rof_pi_default_config(void);

/* ROF_ERR_INVALID_ARG: Zeiger fehlt, kp oder ti_s nicht größer als 0. */
rof_status_t rof_pi_init(rof_pi_t *pi, const rof_pi_config_t *config);

/* Leert den I-Anteil, z. B. beim Eintritt in AUFHEIZEN. */
void rof_pi_reset(rof_pi_t *pi);

/* Ein Regelschritt. dt_s ist die Zeit seit dem letzten Schritt.
 * Liefert den Tastgrad von 0 bis 1, bei ungültigen Werten 0, nie NaN. */
float rof_pi_update(rof_pi_t *pi, float setpoint_c, float measured_c, float dt_s);

#endif /* ROF_PI_H */
