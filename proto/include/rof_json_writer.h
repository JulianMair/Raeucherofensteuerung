/*
 * JSON-Schreiber mit Puffer fester Größe (API-04, E-030).
 *
 * Schreibt JSON Stück für Stück in einen Puffer des Aufrufers, setzt die
 * Kommas selbst und maskiert Sonderzeichen in Texten. Kein malloc.
 * Ist der Puffer zu klein, wird nicht über sein Ende geschrieben:
 * rof_json_writer_ok() liefert dann false, und der Inhalt ist
 * unvollständig, aber immer mit einer Null beendet.
 */
#ifndef ROF_JSON_WRITER_H
#define ROF_JSON_WRITER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *buf;
    size_t size;      /* Größe des Puffers inklusive abschließender Null */
    size_t len;       /* geschriebene Zeichen ohne Null */
    bool overflow;    /* Puffer war zu klein */
    bool needs_comma; /* vor dem nächsten Element steht ein Komma */
} rof_json_writer_t;

void rof_json_writer_init(rof_json_writer_t *w, char *buf, size_t size);
bool rof_json_writer_ok(const rof_json_writer_t *w);

void rof_json_object_begin(rof_json_writer_t *w);
void rof_json_object_end(rof_json_writer_t *w);
void rof_json_array_begin(rof_json_writer_t *w);
void rof_json_array_end(rof_json_writer_t *w);

/* Schreibt "key": ; danach muss genau ein Wert folgen. */
void rof_json_key(rof_json_writer_t *w, const char *key);

void rof_json_string(rof_json_writer_t *w, const char *value);
/* Mit decimals Nachkommastellen; NaN und unendlich werden zu null. */
void rof_json_number(rof_json_writer_t *w, float value, int decimals);
void rof_json_uint(rof_json_writer_t *w, uint64_t value);
void rof_json_bool(rof_json_writer_t *w, bool value);
void rof_json_null(rof_json_writer_t *w);

#endif /* ROF_JSON_WRITER_H */
