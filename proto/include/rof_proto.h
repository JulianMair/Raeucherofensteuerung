/*
 * proto: Anfragen der API in Befehle übersetzen und prüfen, Zustand und
 * Ergebnisse als JSON schreiben (API-02, API-04, E-028, E-030).
 *
 * proto nutzt nur die Typen aus core, nie dessen Funktionen (ARC-04). Die
 * Grenzen für die Prüfung kommen aus dem Snapshot. Der Regeltask prüft
 * jeden Befehl trotzdem noch einmal selbst (ARC-06).
 */
#ifndef ROF_PROTO_H
#define ROF_PROTO_H

#include "rof_command.h"
#include "rof_controller.h"
#include "rof_json_reader.h"

#include <stdbool.h>
#include <stddef.h>

#define ROF_PROTO_MAX_BODY 256     /* größere Anfragen werden nicht gelesen (413) */
#define ROF_PROTO_SENSOR_ID_LEN 16 /* Hex-Ziffern einer Sensor-ID */
#define ROF_PROTO_STATE_JSON_MAX 3072 /* Puffer, in den jeder Zustand passt (getestet) */
#define ROF_PROTO_SMALL_JSON_MAX 128  /* Puffer für Grenzen, Antworten und Fehler */

typedef enum {
    ROF_PROTO_OK = 0,
    ROF_PROTO_BAD_JSON,      /* kein gültiges flaches JSON-Objekt */
    ROF_PROTO_MISSING_FIELD,
    ROF_PROTO_WRONG_TYPE,
    ROF_PROTO_UNKNOWN_FIELD, /* Feld, das diese Anfrage nicht kennt (E-030) */
    ROF_PROTO_UNKNOWN_VALUE, /* z. B. eine unbekannte Rolle */
    ROF_PROTO_BAD_SENSOR_ID,
    ROF_PROTO_TOO_LARGE,
    ROF_PROTO_OUT_OF_RANGE   /* Wert außerhalb der Grenzen, Grund in result */
} rof_proto_status_t;

typedef struct {
    rof_proto_status_t status;
    char field[ROF_JSON_MAX_KEY + 1]; /* betroffenes Feld, sonst leer */
    rof_cmd_result_t result;          /* bei ROF_PROTO_OUT_OF_RANGE der Grund wie im Regeltask */
} rof_proto_result_t;

/* ---- Anfragen dekodieren -----------------------------------------------
 * Bei ROF_PROTO_OK steht der Befehl in *cmd, ohne seq: Die vergibt der
 * Webserver. Sonst bleibt *cmd unverändert. */

/* POST /api/setpoint mit {"setpoint_c": Zahl} */
rof_proto_result_t rof_proto_decode_setpoint(const char *body, size_t len, const rof_limits_t *limits,
                                             rof_command_t *cmd);

/* POST /api/core-target mit {"core_target_c": Zahl} (FC-01) */
rof_proto_result_t rof_proto_decode_core_target(const char *body, size_t len, const rof_limits_t *limits,
                                                rof_command_t *cmd);

/* POST /api/start, /api/stop, /api/reset: kein Inhalt oder {} */
rof_proto_result_t rof_proto_decode_empty(rof_command_type_t type, const char *body, size_t len,
                                          rof_command_t *cmd);

/* PUT /api/sensors/{id}/role mit {"role": Text} */
rof_proto_result_t rof_proto_decode_role(const char *sensor_id, const char *body, size_t len,
                                         rof_command_t *cmd);

/* PUT /api/sensors/{id}/offset mit {"offset_c": Zahl} */
rof_proto_result_t rof_proto_decode_offset(const char *sensor_id, const char *body, size_t len,
                                           const rof_limits_t *limits, rof_command_t *cmd);

/* ---- Antworten kodieren ------------------------------------------------
 * Alle schreiben JSON nach buf (Größe size) und die Länge nach *len.
 * false, wenn der Puffer zu klein war. */

/* GET /api/state und jede Nachricht auf /ws */
bool rof_proto_encode_state(const rof_snapshot_t *snapshot, char *buf, size_t size, size_t *len);

/* GET /api/config */
bool rof_proto_encode_limits(const rof_limits_t *limits, char *buf, size_t size, size_t *len);

/* Ergebnis des Regeltasks: {"seq": ..., "result": ...} */
bool rof_proto_encode_command_response(uint32_t seq, rof_cmd_result_t result, char *buf, size_t size,
                                       size_t *len);

/* Von proto abgelehnt: bei Wert außerhalb {"result": Grund}, sonst {"error": ..., "field": ...} */
bool rof_proto_encode_rejection(const rof_proto_result_t *rejection, char *buf, size_t size, size_t *len);

/* Fehler des Webservers selbst: "busy", "timeout", "not_ready", "foreign_origin" */
bool rof_proto_encode_error(const char *error, bool has_seq, uint32_t seq, char *buf, size_t size,
                            size_t *len);

/* Sucht im Snapshot das Ergebnis des Befehls mit dieser Nummer (E-026,
 * E-037). false, wenn er (noch) nicht oder nicht mehr darin steht; *result
 * bleibt dann unverändert. */
bool rof_proto_command_result(const rof_snapshot_t *snapshot, uint32_t seq, rof_cmd_result_t *result);

/* HTTP-Status für ein Ergebnis des Regeltasks (E-028) */
int rof_proto_http_status(rof_cmd_result_t result);

/* HTTP-Status für eine Ablehnung durch proto: 413 zu groß, 422 außerhalb der Grenzen, sonst 400 */
int rof_proto_rejection_http_status(const rof_proto_result_t *rejection);

/* ---- Sensor-IDs: genau 16 Hex-Ziffern in Großbuchstaben (E-028) -------- */

bool rof_proto_parse_sensor_id(const char *text, rof_sensor_id_t *id);
void rof_proto_format_sensor_id(rof_sensor_id_t id, char out[ROF_PROTO_SENSOR_ID_LEN + 1]);

/* ---- Namen in der API, wie in docs/api.yaml ----------------------------
 * Werte außerhalb der Liste werden zu "unknown", nie zu einem gültigen Namen. */

const char *rof_proto_state_name(rof_state_t state);
const char *rof_proto_role_name(rof_role_t role);
bool rof_proto_role_from_name(const char *name, rof_role_t *role);
const char *rof_proto_role_status_name(rof_role_status_t status);
const char *rof_proto_fault_name(rof_fault_t fault);
const char *rof_proto_read_status_name(rof_status_t status);
const char *rof_proto_command_name(rof_command_type_t type);
const char *rof_proto_result_name(rof_cmd_result_t result);
const char *rof_proto_forecast_status_name(rof_forecast_status_t status);

#endif /* ROF_PROTO_H */
