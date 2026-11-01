#include "rof_json_reader.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Lesezeiger über den Text. end zeigt hinter das letzte Zeichen. */
typedef struct {
    const char *p;
    const char *end;
} cursor_t;

/* ---- Kleine Helfer ------------------------------------------------------ */

static bool at_end(const cursor_t *c)
{
    return c->p >= c->end;
}

/* Nächstes Zeichen, ohne es zu verbrauchen; am Ende '\0' */
static char peek(const cursor_t *c)
{
    return at_end(c) ? '\0' : *c->p;
}

static bool consume(cursor_t *c, char expected)
{
    if (!at_end(c) && *c->p == expected) {
        c->p++;
        return true;
    }
    return false;
}

static bool is_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static void skip_whitespace(cursor_t *c)
{
    while (!at_end(c) && (*c->p == ' ' || *c->p == '\t' || *c->p == '\n' || *c->p == '\r')) {
        c->p++;
    }
}

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

/* ---- Grammatik ---------------------------------------------------------- */

/* Nach einem Backslash: das gemeinte Zeichen nach *out. \uXXXX nur für
 * ASCII ohne Nullzeichen, sonst Syntaxfehler (E-030). */
static rof_json_status_t parse_escape(cursor_t *c, char *out)
{
    if (at_end(c)) {
        return ROF_JSON_SYNTAX;
    }
    const char esc = *c->p++;
    switch (esc) {
    case '"':
    case '\\':
    case '/':
        *out = esc;
        return ROF_JSON_OK;
    case 'b':
        *out = '\b';
        return ROF_JSON_OK;
    case 'f':
        *out = '\f';
        return ROF_JSON_OK;
    case 'n':
        *out = '\n';
        return ROF_JSON_OK;
    case 'r':
        *out = '\r';
        return ROF_JSON_OK;
    case 't':
        *out = '\t';
        return ROF_JSON_OK;
    case 'u': {
        int code = 0;
        for (int i = 0; i < 4; i++) {
            const int digit = at_end(c) ? -1 : hex_value(*c->p++);
            if (digit < 0) {
                return ROF_JSON_SYNTAX;
            }
            code = code * 16 + digit;
        }
        if (code == 0 || code > 0x7F) {
            return ROF_JSON_SYNTAX;
        }
        *out = (char)code;
        return ROF_JSON_OK;
    }
    default:
        return ROF_JSON_SYNTAX;
    }
}

/* Text in Anführungszeichen nach out, höchstens max_len Zeichen. */
static rof_json_status_t parse_string(cursor_t *c, char *out, size_t max_len)
{
    if (!consume(c, '"')) {
        return ROF_JSON_SYNTAX;
    }
    size_t n = 0;
    for (;;) {
        if (at_end(c)) {
            return ROF_JSON_SYNTAX; /* Text nicht beendet */
        }
        char ch = *c->p++;
        if (ch == '"') {
            break;
        }
        if ((unsigned char)ch < 0x20) {
            return ROF_JSON_SYNTAX; /* Steuerzeichen müssen maskiert sein */
        }
        if (ch == '\\') {
            const rof_json_status_t status = parse_escape(c, &ch);
            if (status != ROF_JSON_OK) {
                return status;
            }
        }
        if (n == max_len) {
            return ROF_JSON_TOO_LONG;
        }
        out[n++] = ch;
    }
    out[n] = '\0';
    return ROF_JSON_OK;
}

/* Zahl nach der JSON-Grammatik: -? (0 | [1-9][0-9]*) (.[0-9]+)? ([eE][+-]?[0-9]+)?
 * Erst wenn der Text passt, rechnet strtof ihn um. strtof allein nähme auch
 * +1, 0x10 oder NaN an. */
static rof_json_status_t parse_number(cursor_t *c, float *value)
{
    const char *start = c->p;
    consume(c, '-');
    if (consume(c, '0')) {
        /* nach einer führenden 0 keine weiteren Ziffern */
    } else if (peek(c) >= '1' && peek(c) <= '9') {
        while (is_digit(peek(c))) {
            c->p++;
        }
    } else {
        return ROF_JSON_SYNTAX;
    }
    if (consume(c, '.')) {
        if (!is_digit(peek(c))) {
            return ROF_JSON_SYNTAX;
        }
        while (is_digit(peek(c))) {
            c->p++;
        }
    }
    if (consume(c, 'e') || consume(c, 'E')) {
        if (!consume(c, '+')) {
            consume(c, '-');
        }
        if (!is_digit(peek(c))) {
            return ROF_JSON_SYNTAX;
        }
        while (is_digit(peek(c))) {
            c->p++;
        }
    }
    const size_t n = (size_t)(c->p - start);
    if (n > ROF_JSON_MAX_NUMBER) {
        return ROF_JSON_TOO_LONG;
    }
    char text[ROF_JSON_MAX_NUMBER + 1];
    memcpy(text, start, n);
    text[n] = '\0';
    *value = strtof(text, NULL);
    return ROF_JSON_OK;
}

/* true, false oder null, Zeichen für Zeichen */
static rof_json_status_t parse_literal(cursor_t *c, const char *word)
{
    for (const char *w = word; *w != '\0'; w++) {
        if (!consume(c, *w)) {
            return ROF_JSON_SYNTAX;
        }
    }
    return ROF_JSON_OK;
}

/* Ein Wert. Objekte und Arrays sind nicht vorgesehen und landen im Fehler. */
static rof_json_status_t parse_value(cursor_t *c, rof_json_field_t *field)
{
    const char ch = peek(c);
    if (ch == '"') {
        field->type = ROF_JSON_STRING;
        return parse_string(c, field->string, ROF_JSON_MAX_STRING);
    }
    if (ch == '-' || is_digit(ch)) {
        field->type = ROF_JSON_NUMBER;
        return parse_number(c, &field->number);
    }
    if (ch == 't') {
        field->type = ROF_JSON_TRUE;
        return parse_literal(c, "true");
    }
    if (ch == 'f') {
        field->type = ROF_JSON_FALSE;
        return parse_literal(c, "false");
    }
    if (ch == 'n') {
        field->type = ROF_JSON_NULL;
        return parse_literal(c, "null");
    }
    return ROF_JSON_SYNTAX;
}

/* ---- Öffentliche Funktionen --------------------------------------------- */

rof_json_status_t rof_json_parse_flat(const char *text, size_t len, rof_json_object_t *out)
{
    cursor_t c = { .p = text, .end = text + len };
    out->count = 0;

    skip_whitespace(&c);
    if (!consume(&c, '{')) {
        return ROF_JSON_SYNTAX;
    }
    skip_whitespace(&c);
    if (!consume(&c, '}')) {
        for (;;) {
            if (out->count == ROF_JSON_MAX_FIELDS) {
                return ROF_JSON_TOO_MANY_FIELDS;
            }
            rof_json_field_t *field = &out->fields[out->count];
            field->number = 0.0f;
            field->string[0] = '\0';

            rof_json_status_t status = parse_string(&c, field->key, ROF_JSON_MAX_KEY);
            if (status != ROF_JSON_OK) {
                return status;
            }
            if (rof_json_find(out, field->key) != NULL) {
                return ROF_JSON_DUPLICATE_KEY;
            }
            skip_whitespace(&c);
            if (!consume(&c, ':')) {
                return ROF_JSON_SYNTAX;
            }
            skip_whitespace(&c);
            status = parse_value(&c, field);
            if (status != ROF_JSON_OK) {
                return status;
            }
            out->count++;

            skip_whitespace(&c);
            if (consume(&c, '}')) {
                break;
            }
            if (!consume(&c, ',')) {
                return ROF_JSON_SYNTAX;
            }
            skip_whitespace(&c);
        }
    }
    /* Nach dem Objekt darf nur noch Leerraum kommen */
    skip_whitespace(&c);
    return at_end(&c) ? ROF_JSON_OK : ROF_JSON_SYNTAX;
}

const rof_json_field_t *rof_json_find(const rof_json_object_t *obj, const char *key)
{
    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->fields[i].key, key) == 0) {
            return &obj->fields[i];
        }
    }
    return NULL;
}
