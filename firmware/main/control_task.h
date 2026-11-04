/*
 * Regeltask (ARC-05 bis ARC-09, SAF-08, E-036).
 *
 * Läuft jede Sekunde: DS18B20 abfragen, Regelzyklus, Snapshot
 * veröffentlichen, Watchdog zurücksetzen. Er hat die höchste Priorität
 * der Anwendung und läuft fest auf CPU 1. WLAN und lwIP laufen in ESP-IDF
 * auf CPU 0. Er wartet nie auf andere Tasks: Befehle holt er ohne Warten,
 * den Snapshot legt er nur ab, wenn der Mutex frei ist.
 */
#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include "ds18b20_sensors.h"
#include "rof_controller.h"
#include "snapshot_store.h"

#include <stdint.h>

#define CONTROL_TASK_PERIOD_MS 1000u
#define CONTROL_TASK_PRIORITY 10u /* über Statuslog (1) und HTTP-Server (5, ab M8) */
#define CONTROL_TASK_CORE 1
#define CONTROL_TASK_STACK_BYTES 6144u

typedef struct {
    rof_controller_t *controller;
    ds18b20_sensors_t *sensors;
    snapshot_store_t *store;
    /* Laufzeitwerte für das Statuslog. Nur der Regeltask schreibt sie;
     * 32-Bit-Werte werden auf dem ESP32 in einem Zug gelesen. */
    volatile uint32_t cycles;
    volatile uint32_t last_cycle_us;
    volatile uint32_t max_cycle_us;
    volatile uint32_t stack_free_bytes;
} control_task_t;

/* Startet den Regeltask. task muss die ganze Laufzeit gültig bleiben. */
void control_task_start(control_task_t *task);

#endif /* CONTROL_TASK_H */
