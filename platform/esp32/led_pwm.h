/*
 * Schaltlogik des LED-Ausgangs (CTL-03, SAF-08, E-023, E-034). Reines C,
 * damit die Host-Tests sie prüfen. Den Pin schaltet esp32_led_output.
 *
 * led_pwm_set_duty() merkt sich den Tastgrad und den Zeitpunkt.
 * led_pwm_is_on() sagt für einen Zeitpunkt, ob die LED leuchtet: nach der
 * langsamen PWM aus core, aber nur, solange der Regeltask den Tastgrad
 * auffrischt. Kommt länger als hold_ms kein neuer Tastgrad, ist die LED
 * aus, auch wenn der letzte größer als 0 war. So heizt ein hängender
 * Regeltask nicht weiter, bis der Watchdog neu startet.
 */
#ifndef LED_PWM_H
#define LED_PWM_H

#include "rof_status.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t period_ms; /* Periode der langsamen PWM */
    uint32_t hold_ms;   /* so lange gilt ein Tastgrad ohne Auffrischen */
    bool has_duty;
    float duty;
    uint64_t set_ms;    /* Zeitpunkt des letzten gültigen Tastgrads */
} led_pwm_t;

/* Startet ohne Tastgrad, die LED ist also aus (SAF-10). */
void led_pwm_init(led_pwm_t *pwm, uint32_t period_ms, uint32_t hold_ms);

/* duty von 0.0 bis 1.0. ROF_ERR_INVALID_ARG bei Werten außerhalb oder NaN;
 * dann bleibt alles wie vorher, auch die Haltezeit wird nicht aufgefrischt. */
rof_status_t led_pwm_set_duty(led_pwm_t *pwm, float duty, uint64_t now_ms);

bool led_pwm_is_on(const led_pwm_t *pwm, uint64_t now_ms);

#endif /* LED_PWM_H */
