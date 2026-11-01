/*
 * Befehlsqueue für Host-Tests. Sie verhält sich wie eine FreeRTOS-Queue,
 * in die der Webserver mit Wartezeit 0 schreibt: Ist sie voll, scheitert
 * der Absender sofort, und der Regeltask merkt davon nichts (ARC-09).
 */
#ifndef MOCK_COMMAND_QUEUE_H
#define MOCK_COMMAND_QUEUE_H

#include "rof_command.h"

#include <stdbool.h>
#include <stddef.h>

/* So groß wie die Queue der Firmware (M7) */
#define MOCK_COMMAND_QUEUE_CAPACITY 16

typedef struct {
    rof_command_t items[MOCK_COMMAND_QUEUE_CAPACITY];
    size_t head;  /* nächster abzuholender Befehl */
    size_t count; /* Befehle in der Queue */
    unsigned received;
    unsigned rejected_pushes;
} mock_command_queue_t;

void mock_command_queue_init(mock_command_queue_t *queue);

/* Seite des Absenders. false, wenn die Queue voll ist. */
bool mock_command_queue_push(mock_command_queue_t *queue, rof_command_t command);

/* Seite des Regeltasks */
rof_command_source_t mock_command_queue_source(mock_command_queue_t *queue);

#endif /* MOCK_COMMAND_QUEUE_H */
