/*
 * Tests des JSON-Schreibers in proto (API-04).
 */
#include "unity.h"

#include "rof_json_writer.h"

#include <math.h>
#include <string.h>

static char buf[256];
static rof_json_writer_t w;

void setUp(void)
{
    memset(buf, 'X', sizeof(buf));
    rof_json_writer_init(&w, buf, sizeof(buf));
}

void tearDown(void) {}

static void test_empty_object(void)
{
    rof_json_object_begin(&w);
    rof_json_object_end(&w);

    TEST_ASSERT_TRUE(rof_json_writer_ok(&w));
    TEST_ASSERT_EQUAL_STRING("{}", buf);
    TEST_ASSERT_EQUAL_size_t(2, w.len);
}

/* Kommas setzt der Schreiber selbst, auch über Verschachtelungen hinweg */
static void test_object_with_values_nested_object_and_array(void)
{
    rof_json_object_begin(&w);
    rof_json_key(&w, "a");
    rof_json_uint(&w, 1);
    rof_json_key(&w, "b");
    rof_json_object_begin(&w);
    rof_json_key(&w, "c");
    rof_json_bool(&w, true);
    rof_json_object_end(&w);
    rof_json_key(&w, "d");
    rof_json_array_begin(&w);
    rof_json_string(&w, "x");
    rof_json_null(&w);
    rof_json_bool(&w, false);
    rof_json_array_end(&w);
    rof_json_object_end(&w);

    TEST_ASSERT_EQUAL_STRING("{\"a\":1,\"b\":{\"c\":true},\"d\":[\"x\",null,false]}", buf);
}

static void test_empty_array(void)
{
    rof_json_array_begin(&w);
    rof_json_array_end(&w);

    TEST_ASSERT_EQUAL_STRING("[]", buf);
}

/* Anführungszeichen, Backslash und Steuerzeichen werden maskiert */
static void test_string_escaping(void)
{
    rof_json_string(&w, "a\"b\\c\nd\x01");

    TEST_ASSERT_EQUAL_STRING("\"a\\\"b\\\\c\\nd\\u0001\"", buf);
}

/* Zahlen mit fester Zahl von Nachkommastellen */
static void test_numbers(void)
{
    rof_json_array_begin(&w);
    rof_json_number(&w, 80.0f, 2);
    rof_json_number(&w, -0.5f, 2);
    rof_json_number(&w, 79.944f, 2);
    rof_json_number(&w, 0.4567f, 3);
    rof_json_array_end(&w);

    TEST_ASSERT_EQUAL_STRING("[80.00,-0.50,79.94,0.457]", buf);
}

/* JSON kennt kein NaN und kein unendlich: daraus wird null */
static void test_nan_and_infinity_become_null(void)
{
    rof_json_array_begin(&w);
    rof_json_number(&w, NAN, 2);
    rof_json_number(&w, INFINITY, 2);
    rof_json_number(&w, -INFINITY, 2);
    rof_json_array_end(&w);

    TEST_ASSERT_EQUAL_STRING("[null,null,null]", buf);
}

/* Auch große Zahlen wie Millisekunden seit dem Start */
static void test_large_unsigned(void)
{
    rof_json_uint(&w, 18446744073709551615ULL);

    TEST_ASSERT_EQUAL_STRING("18446744073709551615", buf);
}

/* Passt es genau mit abschließender Null, ist alles gut */
static void test_exact_fit(void)
{
    char small[3];
    rof_json_writer_init(&w, small, sizeof(small));

    rof_json_object_begin(&w);
    rof_json_object_end(&w);

    TEST_ASSERT_TRUE(rof_json_writer_ok(&w));
    TEST_ASSERT_EQUAL_STRING("{}", small);
}

/* Zu kleiner Puffer: gemeldet, nie über das Ende geschrieben, immer mit Null beendet */
static void test_overflow_is_reported_and_buffer_stays_terminated(void)
{
    char small[8];
    memset(small, 'X', sizeof(small));
    char guard = 'G';
    rof_json_writer_init(&w, small, sizeof(small) - 1); /* letztes Byte als Wächter */
    small[sizeof(small) - 1] = guard;

    rof_json_object_begin(&w);
    rof_json_key(&w, "temperature");
    rof_json_number(&w, 80.0f, 2);
    rof_json_object_end(&w);

    TEST_ASSERT_FALSE(rof_json_writer_ok(&w));
    TEST_ASSERT_EQUAL_CHAR(guard, small[sizeof(small) - 1]);
    TEST_ASSERT_TRUE(strlen(small) < sizeof(small) - 1);
}

/* Größe 0: nichts geschrieben, aber gemeldet */
static void test_zero_size_buffer(void)
{
    char none[1] = { 'Z' };
    rof_json_writer_init(&w, none, 0);

    rof_json_null(&w);

    TEST_ASSERT_FALSE(rof_json_writer_ok(&w));
    TEST_ASSERT_EQUAL_CHAR('Z', none[0]);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_empty_object);
    RUN_TEST(test_object_with_values_nested_object_and_array);
    RUN_TEST(test_empty_array);
    RUN_TEST(test_string_escaping);
    RUN_TEST(test_numbers);
    RUN_TEST(test_nan_and_infinity_become_null);
    RUN_TEST(test_large_unsigned);
    RUN_TEST(test_exact_fit);
    RUN_TEST(test_overflow_is_reported_and_buffer_stays_terminated);
    RUN_TEST(test_zero_size_buffer);

    return UNITY_END();
}
