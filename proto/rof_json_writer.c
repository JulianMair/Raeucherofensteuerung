#include "rof_json_writer.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

/* ---- Anhängen ----------------------------------------------------------- */

/* Einzige Stelle, die in den Puffer schreibt. Passt text nicht mehr samt
 * abschließender Null hinein, wird nichts mehr geschrieben. */
static void append(rof_json_writer_t *w, const char *text, size_t n)
{
    if (w->overflow) {
        return;
    }
    if (w->size == 0 || w->len + n >= w->size) {
        w->overflow = true;
        return;
    }
    memcpy(&w->buf[w->len], text, n);
    w->len += n;
    w->buf[w->len] = '\0';
}

static void append_text(rof_json_writer_t *w, const char *text)
{
    append(w, text, strlen(text));
}

/* Vor jedem Wert und jedem Schlüssel: Komma, wenn schon etwas davor steht */
static void begin_element(rof_json_writer_t *w)
{
    if (w->needs_comma) {
        append(w, ",", 1);
    }
}

static void write_escaped(rof_json_writer_t *w, const char *text)
{
    append(w, "\"", 1);
    for (const char *p = text; *p != '\0'; p++) {
        const unsigned char c = (unsigned char)*p;
        if (c == '"') {
            append_text(w, "\\\"");
        } else if (c == '\\') {
            append_text(w, "\\\\");
        } else if (c == '\n') {
            append_text(w, "\\n");
        } else if (c < 0x20) {
            char escaped[8];
            snprintf(escaped, sizeof(escaped), "\\u%04x", c);
            append_text(w, escaped);
        } else {
            append(w, p, 1);
        }
    }
    append(w, "\"", 1);
}

/* ---- Öffentliche Funktionen --------------------------------------------- */

void rof_json_writer_init(rof_json_writer_t *w, char *buf, size_t size)
{
    w->buf = buf;
    w->size = size;
    w->len = 0;
    w->overflow = false;
    w->needs_comma = false;
    if (size > 0) {
        buf[0] = '\0';
    }
}

bool rof_json_writer_ok(const rof_json_writer_t *w)
{
    return !w->overflow;
}

void rof_json_object_begin(rof_json_writer_t *w)
{
    begin_element(w);
    append(w, "{", 1);
    w->needs_comma = false;
}

void rof_json_object_end(rof_json_writer_t *w)
{
    append(w, "}", 1);
    w->needs_comma = true;
}

void rof_json_array_begin(rof_json_writer_t *w)
{
    begin_element(w);
    append(w, "[", 1);
    w->needs_comma = false;
}

void rof_json_array_end(rof_json_writer_t *w)
{
    append(w, "]", 1);
    w->needs_comma = true;
}

void rof_json_key(rof_json_writer_t *w, const char *key)
{
    begin_element(w);
    write_escaped(w, key);
    append(w, ":", 1);
    w->needs_comma = false; /* der Wert folgt ohne Komma */
}

void rof_json_string(rof_json_writer_t *w, const char *value)
{
    begin_element(w);
    write_escaped(w, value);
    w->needs_comma = true;
}

void rof_json_number(rof_json_writer_t *w, float value, int decimals)
{
    begin_element(w);
    /* So formuliert, dass NaN und unendlich zu null werden: JSON kennt beides nicht */
    if (!(value >= -FLT_MAX && value <= FLT_MAX)) {
        append_text(w, "null");
    } else {
        char text[48];
        snprintf(text, sizeof(text), "%.*f", decimals, (double)value);
        append_text(w, text);
    }
    w->needs_comma = true;
}

void rof_json_uint(rof_json_writer_t *w, uint64_t value)
{
    begin_element(w);
    char text[24];
    snprintf(text, sizeof(text), "%llu", (unsigned long long)value);
    append_text(w, text);
    w->needs_comma = true;
}

void rof_json_bool(rof_json_writer_t *w, bool value)
{
    begin_element(w);
    append_text(w, value ? "true" : "false");
    w->needs_comma = true;
}

void rof_json_null(rof_json_writer_t *w)
{
    begin_element(w);
    append_text(w, "null");
    w->needs_comma = true;
}
