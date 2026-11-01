/*
 * Tests des PI-Reglers mit Anti-Windup (CTL-02).
 */
#include "unity.h"

#include "rof_pi.h"

#include <math.h>

static rof_pi_config_t config;
static rof_pi_t pi;

void setUp(void)
{
    config = rof_pi_default_config();
    TEST_ASSERT_EQUAL(ROF_OK, rof_pi_init(&pi, &config));
}

void tearDown(void) {}

/* Standardwerte aus der SIMC-Regel (E-024) */
static void test_default_config_has_agreed_values(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.075f, config.kp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 250.0f, config.ti_s);
}

static void test_init_rejects_bad_config(void)
{
    rof_pi_t other;
    rof_pi_config_t c;

    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_pi_init(NULL, &config));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_pi_init(&other, NULL));

    c = config;
    c.kp = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_pi_init(&other, &c));

    c = config;
    c.ti_s = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_pi_init(&other, &c));

    c = config;
    c.kp = NAN;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_pi_init(&other, &c));
}

/* CTL-02: P-Anteil. Ohne Zeitschritt kein I-Anteil: 0,075 * 10 °C = 0,75 */
static void test_proportional_part(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.75f, rof_pi_update(&pi, 80.0f, 70.0f, 0.0f));
}

/* CTL-02: I-Anteil wächst mit kp * e * dt / ti. 100 s bei 2 °C: 0,075 * 2 * 100 / 250 = 0,06 */
static void test_integral_part_accumulates(void)
{
    float duty = 0.0f;
    for (int i = 0; i < 100; i++) {
        duty = rof_pi_update(&pi, 80.0f, 78.0f, 1.0f);
    }

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.15f + 0.06f, duty);
}

/* CTL-02: der Ausgang bleibt zwischen 0 und 1 */
static void test_output_is_limited_to_zero_and_one(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, rof_pi_update(&pi, 80.0f, 20.0f, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, rof_pi_update(&pi, 80.0f, 100.0f, 1.0f));
}

/* CTL-02, Anti-Windup: eine Stunde am Anschlag lässt den I-Anteil nicht anwachsen.
 * Sobald der Sollwert erreicht ist, geht der Ausgang deshalb sofort zurück,
 * statt lange überzuheizen. */
static void test_integral_does_not_wind_up_while_saturated(void)
{
    for (int i = 0; i < 3600; i++) {
        rof_pi_update(&pi, 80.0f, 20.0f, 1.0f);
    }

    /* Knapp über dem Sollwert: nur der P-Anteil, also unter 0 und damit 0 */
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, rof_pi_update(&pi, 80.0f, 80.5f, 1.0f));
    /* Knapp darunter: kleiner Tastgrad, nicht voll */
    TEST_ASSERT_TRUE(rof_pi_update(&pi, 80.0f, 79.0f, 0.0f) < 0.2f);
}

/* CTL-02, Anti-Windup nach unten: zu warm am Anschlag 0 baut keinen negativen I-Anteil auf */
static void test_integral_does_not_wind_down_while_saturated(void)
{
    for (int i = 0; i < 3600; i++) {
        rof_pi_update(&pi, 80.0f, 110.0f, 1.0f);
    }

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.75f, rof_pi_update(&pi, 80.0f, 70.0f, 0.0f));
}

/* CTL-02: ohne Anschlag baut sich der I-Anteil bei zu hoher Temperatur wieder ab */
static void test_integral_decreases_when_too_warm(void)
{
    for (int i = 0; i < 100; i++) {
        rof_pi_update(&pi, 80.0f, 78.0f, 1.0f); /* I = 0,06 */
    }
    float before = rof_pi_update(&pi, 80.0f, 80.5f, 0.0f);
    float after = 0.0f;
    for (int i = 0; i < 10; i++) {
        after = rof_pi_update(&pi, 80.0f, 80.5f, 1.0f);
    }

    TEST_ASSERT_TRUE(after < before);
}

/* Neustart des Reglers beim Eintritt in AUFHEIZEN: I-Anteil leer */
static void test_reset_clears_integral(void)
{
    for (int i = 0; i < 100; i++) {
        rof_pi_update(&pi, 80.0f, 78.0f, 1.0f);
    }

    rof_pi_reset(&pi);

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.15f, rof_pi_update(&pi, 80.0f, 78.0f, 0.0f));
}

/* Ein ungültiger Messwert oder Zeitschritt ergibt Tastgrad 0, nie NaN */
static void test_invalid_input_gives_zero(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, rof_pi_update(&pi, 80.0f, NAN, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.75f, rof_pi_update(&pi, 80.0f, 70.0f, NAN));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_default_config_has_agreed_values);
    RUN_TEST(test_init_rejects_bad_config);
    RUN_TEST(test_proportional_part);
    RUN_TEST(test_integral_part_accumulates);
    RUN_TEST(test_output_is_limited_to_zero_and_one);
    RUN_TEST(test_integral_does_not_wind_up_while_saturated);
    RUN_TEST(test_integral_does_not_wind_down_while_saturated);
    RUN_TEST(test_integral_decreases_when_too_warm);
    RUN_TEST(test_reset_clears_integral);
    RUN_TEST(test_invalid_input_gives_zero);

    return UNITY_END();
}
