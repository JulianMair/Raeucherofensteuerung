/*
 * Befehle an den Regeltask und ihre Quelle (ARC-05, ARC-06, E-026).
 *
 * Ein Befehl ist ein flaches Struct ohne Zeiger, damit er sich unverändert
 * durch eine FreeRTOS-Queue kopieren lässt (M7). core kennt die Queue
 * nicht, sondern nur das Interface rof_command_source_t.
 */
#ifndef ROF_COMMAND_H
#define ROF_COMMAND_H

#include "rof_sensor_registry.h"
#include "rof_temp_sensor.h"

#include <stdbool.h>
#include <stdint.h>

/* Beginnt bei 1: Ein mit 0 gefüllter Befehl ist kein gültiger Befehl. */
typedef enum {
    ROF_COMMAND_SET_SETPOINT = 1, /* value_c = Sollwert */
    ROF_COMMAND_START,
    ROF_COMMAND_STOP,
    ROF_COMMAND_ASSIGN_ROLE,      /* sensor_id, role */
    ROF_COMMAND_SET_OFFSET,       /* sensor_id, value_c = Offset */
    ROF_COMMAND_RESET             /* sicheren Zustand zurücksetzen */
} rof_command_type_t;

typedef struct {
    uint32_t seq;              /* laufende Nummer des Absenders, kommt im Snapshot zurück */
    rof_command_type_t type;
    float value_c;             /* Sollwert oder Offset */
    rof_sensor_id_t sensor_id; /* für ROLLE ZUWEISEN und OFFSET SETZEN */
    rof_role_t role;           /* für ROLLE ZUWEISEN */
} rof_command_t;

/* Ergebnis eines Befehls. Bei Ablehnung ändert sich nichts, und der Grund
 * wird gemeldet (SAF-09, E-022). */
typedef enum {
    ROF_CMD_OK = 0,
    ROF_CMD_NO_SETPOINT,           /* Start ohne gesetzten Sollwert */
    ROF_CMD_ALREADY_RUNNING,       /* Start während AUFHEIZEN oder HALTEN */
    ROF_CMD_FAULT_ACTIVE,          /* Start in FEHLER */
    ROF_CMD_SAFE_STATE,            /* Start in SICHERER_ZUSTAND */
    ROF_CMD_SETPOINT_TOO_LOW,      /* Sollwert unter der Grenze */
    ROF_CMD_SETPOINT_TOO_HIGH,     /* Sollwert über der Grenze */
    ROF_CMD_SETPOINT_NOT_A_NUMBER  /* Sollwert ist keine Zahl */
} rof_cmd_result_t;

typedef struct {
    void *ctx;
    /* Holt den nächsten Befehl, ohne zu warten. false, wenn keiner da ist. */
    bool (*receive)(void *ctx, rof_command_t *command);
} rof_command_source_t;

static inline bool rof_command_source_receive(const rof_command_source_t *source,
                                              rof_command_t *command)
{
    return source->receive(source->ctx, command);
}

#endif /* ROF_COMMAND_H */
