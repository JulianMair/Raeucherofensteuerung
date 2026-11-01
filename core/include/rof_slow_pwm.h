/*
 * Langsame Pulsweitenmodulation (CTL-03, E-023).
 *
 * Ein Heizelement schaltet man nicht tausendmal pro Sekunde, sondern im
 * Takt von Sekunden. In jedem Fenster der Länge period_ms ist der Ausgang
 * zuerst duty * period_ms lang an und danach aus. Die Funktion hat keinen
 * Zustand: Sie rechnet nur aus Tastgrad und Uhrzeit. Der LED-Treiber (M7)
 * und der Simulator nutzen sie beide.
 */
#ifndef ROF_SLOW_PWM_H
#define ROF_SLOW_PWM_H

#include <stdbool.h>
#include <stdint.h>

#define ROF_SLOW_PWM_DEFAULT_PERIOD_MS 4000u

/* true, wenn der Ausgang zum Zeitpunkt now_ms an sein soll. Ungültige
 * Werte (period_ms 0, duty NaN oder <= 0) schalten aus, nie an. */
bool rof_slow_pwm_on(uint32_t period_ms, float duty, uint64_t now_ms);

#endif /* ROF_SLOW_PWM_H */
