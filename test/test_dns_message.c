/*
 * Tests der DNS-Antworten für das Captive Portal aus firmware/net
 * (NET-02, E-040). Die Anfragen kommen ungeprüft aus dem Netz, deshalb
 * prüfen die meisten Tests kaputte Pakete.
 */
#include "unity.h"

#include "dns_message.h"

#include <string.h>

static const uint8_t AP_IP[4] = { 192, 168, 4, 1 };

static uint8_t query[600];
static size_t query_len;
static uint8_t response[DNS_MESSAGE_MAX];

void setUp(void)
{
    memset(query, 0, sizeof(query));
    memset(response, 0xAA, sizeof(response));
    query_len = 0;
}

void tearDown(void) {}

static void put8(uint8_t value)
{
    query[query_len++] = value;
}

static void put16(uint16_t value)
{
    put8((uint8_t)(value >> 8));
    put8((uint8_t)(value & 0xFFu));
}

/* Kopf einer Anfrage: ID 0x1234, Flags, Anzahl Fragen und Zusatzeinträge */
static void put_header(uint16_t flags, uint16_t questions, uint16_t additional)
{
    put16(0x1234);
    put16(flags);
    put16(questions);
    put16(0); /* Antworten */
    put16(0); /* Autoritäten */
    put16(additional);
}

/* Name als Folge von Labels, z. B. "captive.apple.com" */
static void put_name(const char *name)
{
    while (*name != '\0') {
        const char *dot = strchr(name, '.');
        const size_t len = dot != NULL ? (size_t)(dot - name) : strlen(name);
        put8((uint8_t)len);
        memcpy(&query[query_len], name, len);
        query_len += len;
        name += len;
        if (*name == '.') {
            name++;
        }
    }
    put8(0);
}

static void make_query(const char *name, uint16_t type)
{
    put_header(0x0100, 1, 0); /* Standardanfrage mit RD */
    put_name(name);
    put16(type);
    put16(DNS_CLASS_IN);
}

static size_t answer(void)
{
    return dns_build_answer(query, query_len, AP_IP, response, sizeof(response));
}

static uint16_t get16(size_t offset)
{
    return (uint16_t)((response[offset] << 8) | response[offset + 1]);
}

/* ---- Gültige Anfragen ---------------------------------------------------- */

/* NET-02: Jeder Name zeigt auf den Access Point */
static void test_a_query_gets_access_point_address(void)
{
    make_query("connectivitycheck.gstatic.com", DNS_TYPE_A);
    const size_t question_len = query_len - 12;

    const size_t len = answer();

    TEST_ASSERT_EQUAL_size_t(12 + question_len + 16, len);
    TEST_ASSERT_EQUAL_HEX16(0x1234, get16(0));    /* gleiche ID */
    TEST_ASSERT_EQUAL_HEX16(0x8500, get16(2));    /* Antwort 0x8000, autoritativ 0x0400, RD 0x0100 */
    TEST_ASSERT_EQUAL_UINT16(1, get16(4));        /* eine Frage */
    TEST_ASSERT_EQUAL_UINT16(1, get16(6));        /* eine Antwort */
    TEST_ASSERT_EQUAL_UINT16(0, get16(8));
    TEST_ASSERT_EQUAL_UINT16(0, get16(10));
    TEST_ASSERT_EQUAL_MEMORY(&query[12], &response[12], question_len);

    const size_t a = 12 + question_len;
    TEST_ASSERT_EQUAL_HEX16(0xC00C, get16(a));    /* Zeiger auf den Namen der Frage */
    TEST_ASSERT_EQUAL_UINT16(DNS_TYPE_A, get16(a + 2));
    TEST_ASSERT_EQUAL_UINT16(DNS_CLASS_IN, get16(a + 4));
    TEST_ASSERT_EQUAL_UINT16(0, get16(a + 6));
    TEST_ASSERT_EQUAL_UINT16(DNS_TTL_S, get16(a + 8));
    TEST_ASSERT_EQUAL_UINT16(4, get16(a + 10));
    TEST_ASSERT_EQUAL_MEMORY(AP_IP, &response[a + 12], 4);
}

/* Ohne RD in der Anfrage auch ohne RD in der Antwort */
static void test_recursion_flag_is_copied(void)
{
    put_header(0x0000, 1, 0);
    put_name("a.b");
    put16(DNS_TYPE_A);
    put16(DNS_CLASS_IN);
    TEST_ASSERT_TRUE(answer() > 0);
    TEST_ASSERT_EQUAL_HEX16(0x8400, get16(2));
}

/* IPv6 und alles andere: gültige Antwort ohne Eintrag, damit das Gerät
 * nicht auf eine Zeitüberschreitung wartet und IPv4 nimmt */
static void test_other_types_get_empty_answer(void)
{
    make_query("captive.apple.com", DNS_TYPE_AAAA);
    const size_t question_len = query_len - 12;
    TEST_ASSERT_EQUAL_size_t(12 + question_len, answer());
    TEST_ASSERT_EQUAL_UINT16(1, get16(4));
    TEST_ASSERT_EQUAL_UINT16(0, get16(6));
}

static void test_other_class_gets_empty_answer(void)
{
    put_header(0x0100, 1, 0);
    put_name("a.b");
    put16(DNS_TYPE_A);
    put16(3); /* CHAOS */
    TEST_ASSERT_EQUAL_size_t(query_len, answer());
    TEST_ASSERT_EQUAL_UINT16(0, get16(6));
}

/* Viele Geräte hängen einen EDNS-Eintrag an. Er wird nicht beantwortet. */
static void test_additional_records_are_ignored(void)
{
    put_header(0x0100, 1, 1);
    put_name("example.com");
    put16(DNS_TYPE_A);
    put16(DNS_CLASS_IN);
    const size_t question_end = query_len;
    put8(0);       /* OPT-Eintrag: leerer Name */
    put16(41);     /* Typ OPT */
    put16(1232);   /* UDP-Größe */
    put16(0);
    put16(0);
    put16(0);      /* keine Daten */

    const size_t len = answer();

    TEST_ASSERT_EQUAL_size_t(question_end + 16, len);
    TEST_ASSERT_EQUAL_UINT16(0, get16(10));
}

static void test_root_name_is_answered(void)
{
    make_query("", DNS_TYPE_A);
    TEST_ASSERT_EQUAL_size_t(12 + 5 + 16, answer());
}

/* ---- Verworfene Pakete --------------------------------------------------- */

static void assert_dropped(void)
{
    TEST_ASSERT_EQUAL_size_t(0, answer());
}

static void test_short_packet_is_dropped(void)
{
    put_header(0x0100, 1, 0);
    query_len = 11;
    assert_dropped();
}

/* Eine Antwort beantworten wir nicht, sonst ließe sich ein Kreislauf bauen */
static void test_response_is_dropped(void)
{
    put_header(0x8100, 1, 0);
    put_name("a.b");
    put16(DNS_TYPE_A);
    put16(DNS_CLASS_IN);
    assert_dropped();
}

static void test_other_opcode_is_dropped(void)
{
    put_header(0x2800, 1, 0); /* UPDATE */
    put_name("a.b");
    put16(DNS_TYPE_A);
    put16(DNS_CLASS_IN);
    assert_dropped();
}

static void test_question_count_must_be_one(void)
{
    put_header(0x0100, 2, 0);
    put_name("a.b");
    put16(DNS_TYPE_A);
    put16(DNS_CLASS_IN);
    put_name("c.d");
    put16(DNS_TYPE_A);
    put16(DNS_CLASS_IN);
    assert_dropped();

    setUp();
    put_header(0x0100, 0, 0);
    assert_dropped();
}

static void test_answers_in_query_are_dropped(void)
{
    make_query("a.b", DNS_TYPE_A);
    query[7] = 1; /* ANCOUNT 1 */
    assert_dropped();
}

static void test_truncated_name_is_dropped(void)
{
    put_header(0x0100, 1, 0);
    put8(10); /* Label mit 10 Zeichen, aber nur 3 da */
    put8('a');
    put8('b');
    put8('c');
    assert_dropped();
}

static void test_missing_type_and_class_are_dropped(void)
{
    put_header(0x0100, 1, 0);
    put_name("a.b");
    put16(DNS_TYPE_A); /* Klasse fehlt */
    assert_dropped();
}

static void test_label_longer_than_63_is_dropped(void)
{
    put_header(0x0100, 1, 0);
    put8(64);
    for (int i = 0; i < 64; i++) {
        put8('a');
    }
    put8(0);
    put16(DNS_TYPE_A);
    put16(DNS_CLASS_IN);
    assert_dropped();
}

static void test_name_longer_than_255_is_dropped(void)
{
    put_header(0x0100, 1, 0);
    for (int label = 0; label < 5; label++) { /* 5 * 51 Byte = 255, plus Null = 256 */
        put8(50);
        for (int i = 0; i < 50; i++) {
            put8('a');
        }
    }
    put8(0);
    put16(DNS_TYPE_A);
    put16(DNS_CLASS_IN);
    assert_dropped();
}

/* In einer Frage hat ein Kompressionszeiger nichts verloren. Die beiden
 * oberen Bits markieren ihn. Damit der Test nicht schon an der Länge des
 * Pakets scheitert, folgen genug Bytes für ein Label dieser Länge. */
static void test_compression_pointer_is_dropped(void)
{
    put_header(0x0100, 1, 0);
    put8(0xC0);
    for (int i = 0; i < 0xC0; i++) {
        put8('a');
    }
    put8(0);
    put16(DNS_TYPE_A);
    put16(DNS_CLASS_IN);
    assert_dropped();
}

static void test_too_small_response_buffer_is_dropped(void)
{
    make_query("connectivitycheck.gstatic.com", DNS_TYPE_A);
    const size_t needed = query_len + 16;
    TEST_ASSERT_EQUAL_size_t(0, dns_build_answer(query, query_len, AP_IP, response, needed - 1));
    TEST_ASSERT_EQUAL_size_t(needed, dns_build_answer(query, query_len, AP_IP, response, needed));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_query_gets_access_point_address);
    RUN_TEST(test_recursion_flag_is_copied);
    RUN_TEST(test_other_types_get_empty_answer);
    RUN_TEST(test_other_class_gets_empty_answer);
    RUN_TEST(test_additional_records_are_ignored);
    RUN_TEST(test_root_name_is_answered);
    RUN_TEST(test_short_packet_is_dropped);
    RUN_TEST(test_response_is_dropped);
    RUN_TEST(test_other_opcode_is_dropped);
    RUN_TEST(test_question_count_must_be_one);
    RUN_TEST(test_answers_in_query_are_dropped);
    RUN_TEST(test_truncated_name_is_dropped);
    RUN_TEST(test_missing_type_and_class_are_dropped);
    RUN_TEST(test_label_longer_than_63_is_dropped);
    RUN_TEST(test_name_longer_than_255_is_dropped);
    RUN_TEST(test_compression_pointer_is_dropped);
    RUN_TEST(test_too_small_response_buffer_is_dropped);
    return UNITY_END();
}
