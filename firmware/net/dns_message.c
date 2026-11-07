#include "dns_message.h"

#include <stdbool.h>
#include <string.h>

/* Aufbau nach RFC 1035: 12 Byte Kopf, dann Frage, Antworten, Zusätze */
#define HEADER_LEN 12u
#define FLAG_RESPONSE 0x80u  /* Byte 2: QR, 1 heißt Antwort */
#define FLAG_AUTHORITY 0x04u /* Byte 2: AA, wir sind für jeden Namen zuständig */
#define FLAG_RECURSION 0x01u /* Byte 2: RD, aus der Anfrage übernommen */
#define OPCODE_MASK 0x78u    /* Byte 2: 0 heißt normale Anfrage */
#define LABEL_MAX 63u
#define NAME_MAX 255u        /* ganzer Name mit Längenbytes und Null */
#define ANSWER_LEN 16u       /* Zeiger 2, Typ 2, Klasse 2, TTL 4, Länge 2, Adresse 4 */

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void write16(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)((value >> 8) & 0xFFu);
    p[1] = (uint8_t)(value & 0xFFu);
}

/* Länge des Namens ab offset bis einschließlich der Null, 0 wenn er kaputt
 * ist. Kompressionszeiger (die beiden oberen Bits gesetzt) kommen in einer
 * Frage nicht vor und gelten als kaputt. */
static size_t name_length(const uint8_t *msg, size_t len, size_t offset)
{
    size_t pos = offset;
    for (;;) {
        if (pos >= len) {
            return 0;
        }
        const uint8_t label = msg[pos];
        if (label == 0) {
            break;
        }
        if (label > LABEL_MAX) {
            return 0;
        }
        pos += 1u + label;
    }
    const size_t name_len = pos + 1u - offset;
    return name_len <= NAME_MAX ? name_len : 0;
}

size_t dns_build_answer(const uint8_t *query, size_t len, const uint8_t ipv4[4], uint8_t *response,
                        size_t size)
{
    if (len < HEADER_LEN) {
        return 0;
    }
    const uint8_t flags = query[2];
    if ((flags & FLAG_RESPONSE) != 0 || (flags & OPCODE_MASK) != 0) {
        return 0;
    }
    /* Genau eine Frage, keine Antworten oder Autoritäten. Zusätze wie EDNS
     * dürfen folgen, sie werden nicht beantwortet. */
    if (read16(&query[4]) != 1 || read16(&query[6]) != 0 || read16(&query[8]) != 0) {
        return 0;
    }
    const size_t name_len = name_length(query, len, HEADER_LEN);
    if (name_len == 0) {
        return 0;
    }
    const size_t question_len = name_len + 4u; /* Typ und Klasse */
    if (HEADER_LEN + question_len > len) {
        return 0;
    }
    const uint8_t *type_class = &query[HEADER_LEN + name_len];
    const bool is_a = read16(type_class) == DNS_TYPE_A && read16(type_class + 2) == DNS_CLASS_IN;
    const size_t answer_len = HEADER_LEN + question_len + (is_a ? ANSWER_LEN : 0u);
    if (answer_len > size) {
        return 0;
    }

    /* Kopf: gleiche ID, Antwort, autoritativ, RD wie angefragt, kein Fehler */
    memcpy(response, query, 2);
    response[2] = (uint8_t)(FLAG_RESPONSE | FLAG_AUTHORITY | (flags & FLAG_RECURSION));
    response[3] = 0;
    write16(&response[4], 1);
    write16(&response[6], is_a ? 1u : 0u);
    write16(&response[8], 0);
    write16(&response[10], 0);
    /* Frage unverändert zurück */
    memcpy(&response[HEADER_LEN], &query[HEADER_LEN], question_len);
    if (is_a) {
        uint8_t *a = &response[HEADER_LEN + question_len];
        write16(&a[0], 0xC000u | HEADER_LEN); /* Zeiger auf den Namen der Frage */
        write16(&a[2], DNS_TYPE_A);
        write16(&a[4], DNS_CLASS_IN);
        write16(&a[6], 0);                    /* TTL, obere 16 Bit */
        write16(&a[8], DNS_TTL_S);            /* TTL, untere 16 Bit */
        write16(&a[10], 4);                   /* Länge der Adresse */
        memcpy(&a[12], ipv4, 4);
    }
    return answer_len;
}
