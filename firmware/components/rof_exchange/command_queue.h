/*
 * Befehlsqueue zwischen Netzwerk und Regeltask (ARC-05, ARC-09, E-026).
 *
 * Eine FreeRTOS-Queue mit festem Speicher, angelegt beim Start. Der
 * Regeltask holt Befehle über rof_command_source_t, der Webserver (M8)
 * legt sie mit command_queue_send() hinein. Keine Seite wartet: Ist die
 * Queue voll, scheitert das Senden sofort, und der Webserver antwortet
 * mit "busy".
 */
#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include "rof_command.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <stdbool.h>
#include <stdint.h>

#define COMMAND_QUEUE_LENGTH 16u

typedef struct {
    StaticQueue_t control; /* Verwaltungsdaten der Queue */
    uint8_t storage[COMMAND_QUEUE_LENGTH * sizeof(rof_command_t)];
    QueueHandle_t handle;
} command_queue_t;

void command_queue_init(command_queue_t *queue);

/* Legt eine Kopie des Befehls in die Queue. false, wenn sie voll ist. */
bool command_queue_send(command_queue_t *queue, const rof_command_t *command);

/* Die Befehlsquelle für den Regelzyklus. Der Zeiger muss gültig bleiben. */
rof_command_source_t command_queue_source(command_queue_t *queue);

#endif /* COMMAND_QUEUE_H */
