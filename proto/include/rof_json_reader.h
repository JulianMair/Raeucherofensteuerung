/*
 * Strenger Leser für flache JSON-Objekte (API-04, E-030).
 *
 * Jede Anfrage an die API ist ein flaches Objekt wie {"setpoint_c": 80}.
 * Genau das liest dieser Leser, sonst nichts:
 *   { "schlüssel": Wert, ... }   mit Wert = Zahl, Text, true, false, null
 * Abgelehnt wird alles andere: verschachtelte Objekte und Arrays, Zahlen
 * außerhalb der JSON-Grammatik (+1, 01, 0x10, NaN), Zeichen nach dem
 * Objekt, doppelte Schlüssel, mehr als ROF_JSON_MAX_FIELDS Felder.
 * \uXXXX ist nur für ASCII erlaubt. Kein malloc: Das Ergebnis steht in
 * einem Struct fester Größe des Aufrufers.
 */
#ifndef ROF_JSON_READER_H
#define ROF_JSON_READER_H

#include <stddef.h>

#define ROF_JSON_MAX_FIELDS 4
#define ROF_JSON_MAX_KEY 24
#define ROF_JSON_MAX_STRING 32
#define ROF_JSON_MAX_NUMBER 32 /* Zeichen einer Zahl im Text */

typedef enum {
    ROF_JSON_NUMBER,
    ROF_JSON_STRING,
    ROF_JSON_TRUE,
    ROF_JSON_FALSE,
    ROF_JSON_NULL
} rof_json_type_t;

typedef struct {
    char key[ROF_JSON_MAX_KEY + 1];
    rof_json_type_t type;
    float number;                         /* bei ROF_JSON_NUMBER, kann unendlich sein */
    char string[ROF_JSON_MAX_STRING + 1]; /* bei ROF_JSON_STRING */
} rof_json_field_t;

typedef struct {
    rof_json_field_t fields[ROF_JSON_MAX_FIELDS];
    size_t count;
} rof_json_object_t;

typedef enum {
    ROF_JSON_OK = 0,
    ROF_JSON_SYNTAX,          /* kein gültiges JSON oder kein flaches Objekt */
    ROF_JSON_TOO_MANY_FIELDS,
    ROF_JSON_TOO_LONG,        /* Schlüssel, Text oder Zahl zu lang */
    ROF_JSON_DUPLICATE_KEY
} rof_json_status_t;

/* Liest genau len Zeichen ab text; text muss nicht mit 0 enden. */
rof_json_status_t rof_json_parse_flat(const char *text, size_t len, rof_json_object_t *out);

/* Feld mit diesem Schlüssel oder NULL. */
const rof_json_field_t *rof_json_find(const rof_json_object_t *obj, const char *key);

#endif /* ROF_JSON_READER_H */
