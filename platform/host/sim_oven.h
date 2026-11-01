/*
 * Simulator eines Räucherofens für Host-Tests (CTL-01, E-025).
 *
 * Garraum: Modell erster Ordnung mit Totzeit. Die Heizung schaltet über
 * die langsame PWM aus core an und aus und wirkt erst nach chamber_dead_s.
 *   dT/dt = (T_umgebung + K * heizung(t - Totzeit) - T) / tau
 * Kern: folgt dem Garraum, ebenfalls erster Ordnung mit eigener Totzeit.
 *
 * Gerechnet wird in festen Schritten von SIM_OVEN_STEP_MS mit double,
 * weil der Simulator nur auf dem Host läuft. Er stellt zwei Sensoren, den
 * Ausgang und die Uhr über die Interfaces aus core bereit; die simulierte
 * Zeit läuft nur mit sim_oven_advance().
 */
#ifndef SIM_OVEN_H
#define SIM_OVEN_H

#include "rof_clock.h"
#include "rof_output.h"
#include "rof_temp_sensor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SIM_OVEN_STEP_MS 100u
/* Längste Totzeit, die der Ringpuffer fasst: 600 s bei 100 ms je Schritt */
#define SIM_OVEN_MAX_DEAD_STEPS 6000u

#define SIM_OVEN_CHAMBER_ID 0x2800000000000C01ULL
#define SIM_OVEN_PROBE_ID 0x2800000000000C02ULL

typedef struct {
    float ambient_c;        /* Umgebungstemperatur */
    float heater_gain_c;    /* Erwärmung über Umgebung bei Dauer-An */
    float chamber_tau_s;    /* Zeitkonstante Garraum */
    float chamber_dead_s;   /* Totzeit Heizung -> Garraum */
    float probe_tau_s;      /* Zeitkonstante Kern */
    float probe_dead_s;     /* Totzeit Garraum -> Kern */
    uint32_t pwm_period_ms; /* Periode der langsamen PWM */
} sim_oven_config_t;

typedef struct {
    sim_oven_config_t config;
    size_t chamber_dead_steps;
    size_t probe_dead_steps;
    uint64_t now_ms;
    double chamber_c;
    double probe_c;
    float duty; /* zuletzt gültig gesetzter Tastgrad */
    /* Ringpuffer für die Totzeiten: Heizung an/aus und Garraumtemperatur */
    uint8_t heater_history[SIM_OVEN_MAX_DEAD_STEPS];
    double chamber_history[SIM_OVEN_MAX_DEAD_STEPS];
    size_t history_pos;
    /* Fehler, die ein Test einspeisen kann */
    bool chamber_unplugged;
    bool chamber_frozen;
    float frozen_c;
    bool heater_stuck_on;
} sim_oven_t;

sim_oven_config_t sim_oven_default_config(void);

/* ROF_ERR_INVALID_ARG: Zeitkonstante nicht größer als 0, Totzeit negativ
 * oder länger als der Puffer, PWM-Periode 0. */
rof_status_t sim_oven_init(sim_oven_t *sim, const sim_oven_config_t *config);

/* Lässt delta_ms simulierte Zeit in Schritten von SIM_OVEN_STEP_MS laufen. */
void sim_oven_advance(sim_oven_t *sim, uint64_t delta_ms);

/* Ab jetzt meldet der Garraumsensor immer den aktuellen Wert. */
void sim_oven_freeze_chamber(sim_oven_t *sim);

rof_temp_sensor_t sim_oven_sensors(sim_oven_t *sim);
rof_output_t sim_oven_output(sim_oven_t *sim);
rof_clock_t sim_oven_clock(sim_oven_t *sim);

#endif /* SIM_OVEN_H */
