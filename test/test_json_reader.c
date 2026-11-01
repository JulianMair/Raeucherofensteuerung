/*
 * Tests des strengen Lesers für flache JSON-Objekte in proto (API-04).
 * Ein großer Teil prüft kaputtes JSON: Es muss sauber abgelehnt werden.
 */
#include "unity.h"

#include "rof_json_reader.h"

#include <string.h>

static rof_json_object_t obj;

void setUp(void)
{
    memset(&obj, 0xAB, sizeof(obj));
}

void tearDown(void) {}

static rof_json_status_t parse(const char *text)
{
    return rof_json_parse_flat(text, strlen(text), &obj);
}

/* Feld, das es geben muss. Die Prüfung steht hier und nicht im Test, weil
 * cppcheck nicht weiß, dass Unity bei einem Fehlschlag den Test verlässt. */
static const rof_json_field_t *field(const char *key)
{
    const rof_json_field_t *f = rof_json_find(&obj, key);
    TEST_ASSERT_NOT_NULL_MESSAGE(f, key);
    return f;
}

/* ---- Gültige Objekte ---------------------------------------------------- */

static void test_empty_object(void)
{
    TEST_ASSERT_EQUAL(ROF_JSON_OK, parse("{}"));
    TEST_ASSERT_EQUAL_size_t(0, obj.count);
}

static void test_number_field(void)
{
    TEST_ASSERT_EQUAL(ROF_JSON_OK, parse("{\"setpoint_c\":80}"));
    TEST_ASSERT_EQUAL_size_t(1, obj.count);
    const rof_json_field_t *f = field("setpoint_c");
    TEST_ASSERT_EQUAL(ROF_JSON_NUMBER, f->type);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 80.0f, f->number);
}

/* Leerzeichen, Tabs und Zeilenumbrüche sind überall zwischen den Teilen erlaubt */
static void test_whitespace_and_several_fields(void)
{
    TEST_ASSERT_EQUAL(ROF_JSON_OK, parse(" {\n\t\"a\" : -1.5e1 , \"b\":\"x\",\"c\" :true, \"d\": null } \r\n"));
    TEST_ASSERT_EQUAL_size_t(4, obj.count);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -15.0f, field("a")->number);
    TEST_ASSERT_EQUAL_STRING("x", field("b")->string);
    TEST_ASSERT_EQUAL(ROF_JSON_TRUE, field("c")->type);
    TEST_ASSERT_EQUAL(ROF_JSON_NULL, field("d")->type);
}

static void test_false_and_number_forms(void)
{
    TEST_ASSERT_EQUAL(ROF_JSON_OK, parse("{\"f\":false,\"z\":0,\"m\":-0.25,\"e\":2E+2}"));
    TEST_ASSERT_EQUAL(ROF_JSON_FALSE, field("f")->type);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, field("z")->number);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.25f, field("m")->number);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 200.0f, field("e")->number);
}

static void test_string_escapes(void)
{
    TEST_ASSERT_EQUAL(ROF_JSON_OK, parse("{\"s\":\"a\\\"b\\\\c\\/d\\n\\u0041\"}"));
    TEST_ASSERT_EQUAL_STRING("a\"b\\c/d\nA", field("s")->string);
}

/* Nur len Zeichen zählen, der Text muss nicht mit 0 enden */
static void test_length_is_respected(void)
{
    const char text[] = "{}garbage";
    TEST_ASSERT_EQUAL(ROF_JSON_OK, rof_json_parse_flat(text, 2, &obj));
}

static void test_find_missing_key_is_null(void)
{
    parse("{\"a\":1}");
    TEST_ASSERT_NULL(rof_json_find(&obj, "b"));
}

/* ---- Kaputtes JSON (API-04) --------------------------------------------- */

static void check_syntax_error(const char *text)
{
    TEST_ASSERT_EQUAL_MESSAGE(ROF_JSON_SYNTAX, parse(text), text);
}

static void test_empty_or_not_an_object(void)
{
    check_syntax_error("");
    check_syntax_error("   ");
    check_syntax_error("[]");
    check_syntax_error("80");
    check_syntax_error("\"x\"");
    check_syntax_error("null");
}

static void test_unterminated_input(void)
{
    check_syntax_error("{");
    check_syntax_error("{\"a\":1");
    check_syntax_error("{\"a\":\"x}");
    check_syntax_error("{\"a\"");
    check_syntax_error("{\"a\":");
}

static void test_missing_or_extra_punctuation(void)
{
    check_syntax_error("{\"a\" 1}");
    check_syntax_error("{\"a\":1 \"b\":2}");
    check_syntax_error("{\"a\":1,}");
    check_syntax_error("{,\"a\":1}");
    check_syntax_error("{a:1}");
    check_syntax_error("{'a':1}");
}

static void test_trailing_content(void)
{
    check_syntax_error("{} x");
    check_syntax_error("{}{}");
}

/* Verschachtelte Objekte und Arrays braucht keine Anfrage, also abgelehnt */
static void test_nested_values_are_rejected(void)
{
    check_syntax_error("{\"a\":{}}");
    check_syntax_error("{\"a\":[1]}");
}

/* Nur die JSON-Grammatik für Zahlen, nicht alles, was strtof versteht */
static void test_invalid_numbers(void)
{
    check_syntax_error("{\"a\":+1}");
    check_syntax_error("{\"a\":01}");
    check_syntax_error("{\"a\":1.}");
    check_syntax_error("{\"a\":.5}");
    check_syntax_error("{\"a\":0x10}");
    check_syntax_error("{\"a\":NaN}");
    check_syntax_error("{\"a\":Infinity}");
    check_syntax_error("{\"a\":-}");
    check_syntax_error("{\"a\":1e}");
}

static void test_invalid_literals_and_strings(void)
{
    check_syntax_error("{\"a\":tru}");
    check_syntax_error("{\"a\":nul}");
    check_syntax_error("{\"a\":\"x\ty\"}");     /* Steuerzeichen roh im Text */
    check_syntax_error("{\"a\":\"\\x\"}");      /* unbekannte Maskierung */
    check_syntax_error("{\"a\":\"\\u00e4\"}");  /* nur ASCII per \u, siehe E-030 */
    check_syntax_error("{\"a\":\"\\u12\"}");    /* zu kurz */
}

/* ---- Grenzen ------------------------------------------------------------ */

static void test_too_many_fields(void)
{
    TEST_ASSERT_EQUAL(ROF_JSON_TOO_MANY_FIELDS, parse("{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5}"));
}

static void test_too_long_key_string_or_number(void)
{
    TEST_ASSERT_EQUAL(ROF_JSON_TOO_LONG, parse("{\"abcdefghijklmnopqrstuvwxy\":1}"));
    TEST_ASSERT_EQUAL(ROF_JSON_TOO_LONG, parse("{\"a\":\"abcdefghijklmnopqrstuvwxyzabcdefg\"}"));
    TEST_ASSERT_EQUAL(ROF_JSON_TOO_LONG, parse("{\"a\":1.000000000000000000000000000000001}"));
}

static void test_duplicate_key(void)
{
    TEST_ASSERT_EQUAL(ROF_JSON_DUPLICATE_KEY, parse("{\"a\":1,\"a\":2}"));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_empty_object);
    RUN_TEST(test_number_field);
    RUN_TEST(test_whitespace_and_several_fields);
    RUN_TEST(test_false_and_number_forms);
    RUN_TEST(test_string_escapes);
    RUN_TEST(test_length_is_respected);
    RUN_TEST(test_find_missing_key_is_null);

    RUN_TEST(test_empty_or_not_an_object);
    RUN_TEST(test_unterminated_input);
    RUN_TEST(test_missing_or_extra_punctuation);
    RUN_TEST(test_trailing_content);
    RUN_TEST(test_nested_values_are_rejected);
    RUN_TEST(test_invalid_numbers);
    RUN_TEST(test_invalid_literals_and_strings);

    RUN_TEST(test_too_many_fields);
    RUN_TEST(test_too_long_key_string_or_number);
    RUN_TEST(test_duplicate_key);

    return UNITY_END();
}
