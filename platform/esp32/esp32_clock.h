/*
 * Uhr für core auf dem ESP32 (ARC-03): Millisekunden seit dem Start aus
 * esp_timer. Der Zähler hat 64 Bit und läuft praktisch nie über.
 */
#ifndef ESP32_CLOCK_H
#define ESP32_CLOCK_H

#include "rof_clock.h"

rof_clock_t esp32_clock_interface(void);

#endif /* ESP32_CLOCK_H */
