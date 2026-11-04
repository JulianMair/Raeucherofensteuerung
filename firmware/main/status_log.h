/*
 * Statuslog (E-036): liest alle 5 s die Snapshot-Kopie und schreibt Zustand,
 * Sensoren und Laufzeitwerte ins serielle Log. In M7 der einzige Weg, die
 * Firmware ohne Netzwerk zu beobachten. Er ist ein Leser wie später der
 * Webserver und hat die niedrigste Priorität.
 */
#ifndef STATUS_LOG_H
#define STATUS_LOG_H

#include "control_task.h"
#include "snapshot_store.h"

#define STATUS_LOG_PERIOD_MS 5000u
#define STATUS_LOG_PRIORITY 1u
#define STATUS_LOG_STACK_BYTES 4096u

/* Beide Zeiger müssen die ganze Laufzeit gültig bleiben. */
void status_log_start(snapshot_store_t *store, const control_task_t *control);

#endif /* STATUS_LOG_H */
