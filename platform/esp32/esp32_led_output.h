/*
 * LED als Ausgang statt Heizelement (CTL-03, SAF-08, SAF-10, E-023, E-034).
 *
 * Ein esp_timer prüft alle ESP32_LED_TICK_MS mit led_pwm, ob die LED an
 * sein soll, und schaltet den Pin. set_duty() kommt aus dem Regeltask, der
 * Timer läuft im esp_timer-Task, unter Umständen auf dem anderen Kern.
 * Deshalb schützt ein Spinlock den gemeinsamen Zustand.
 *
 * Die LED hängt mit Vorwiderstand zwischen Pin und Masse: Pegel 1 heißt an.
 */
#ifndef ESP32_LED_OUTPUT_H
#define ESP32_LED_OUTPUT_H

#include "led_pwm.h"
#include "rof_clock.h"
#include "rof_output.h"

#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

/* 50 ms bei 4 s Periode ergeben 1,25 Prozent Auflösung des Tastgrads */
#define ESP32_LED_TICK_MS 50u

typedef struct {
    int gpio;
    const rof_clock_t *clk;
    led_pwm_t pwm;          /* nur unter lock lesen oder schreiben */
    portMUX_TYPE lock;
    esp_timer_handle_t timer;
} esp32_led_output_t;

/* Schaltet die LED aus und startet den Timer. hold_ms: so lange gilt ein
 * Tastgrad ohne Auffrischen. Bei einem Fehler darf die Regelung nicht
 * starten, der Ausgang wäre nicht steuerbar (E-033). */
esp_err_t esp32_led_output_init(esp32_led_output_t *led, int gpio, const rof_clock_t *clk,
                                uint32_t period_ms, uint32_t hold_ms);

/* Das Interface für core. Der Zeiger muss gültig bleiben. */
rof_output_t esp32_led_output_interface(esp32_led_output_t *led);

#endif /* ESP32_LED_OUTPUT_H */
