/*
 * Ablage der Snapshot-Kopie für die Außenwelt (ARC-07, ARC-09, E-026, E-036).
 *
 * Der Regeltask legt nach jedem Zyklus eine Kopie ab, Leser wie das
 * Statuslog oder der Webserver (M8) holen sich davon eine eigene Kopie.
 * Ein Mutex schützt die Ablage. Der Regeltask wartet nie auf ihn: Ist er
 * belegt, veröffentlicht er erst im nächsten Zyklus wieder.
 */
#ifndef SNAPSHOT_STORE_H
#define SNAPSHOT_STORE_H

#include "rof_controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    StaticSemaphore_t mutex_buffer;
    SemaphoreHandle_t mutex;
    rof_snapshot_t latest; /* nur unter dem Mutex lesen oder schreiben */
    bool has_snapshot;     /* ebenso */
    volatile uint32_t skipped; /* so oft war der Mutex belegt, nur der Regeltask schreibt */
} snapshot_store_t;

void snapshot_store_init(snapshot_store_t *store);

/* Für den Regeltask, wartet nie. false, wenn der Mutex belegt war; dann
 * bleibt die vorige Kopie stehen. */
bool snapshot_store_publish(snapshot_store_t *store, const rof_snapshot_t *snapshot);

/* Für Leser. Wartet höchstens wait auf den Mutex. false, wenn noch nichts
 * veröffentlicht wurde oder die Zeit abgelaufen ist. */
bool snapshot_store_read(snapshot_store_t *store, rof_snapshot_t *out, TickType_t wait);

#endif /* SNAPSHOT_STORE_H */
