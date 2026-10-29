/*
 * Tests der Sicherheitslogik core/safety (SAF-02 bis SAF-12).
 * Jede Regel hat mindestens einen Test für Auslösen und einen für
 * Nichtauslösen (SAF-11). Jeder Test nennt im Kommentar seine Anforderung.
 */
#include "unity.h"

#include "fake_clock.h"
#include "mock_log.h"
#include "rof_safety.h"

#include <math.h>

static fake_clock_t clock_fake;
static rof_clock_t clk_if;
static mock_log_t log_mock;
static rof_log_t logger_if;
static rof_safety_config_t config;
static rof_safety_t safety;

void setUp(void)
{
    fake_clock_init(&clock_fake, 0);
    clk_if = fake_clock_interface(&clock_fake);
    mock_log_init(&log_mock);
    logger_if = mock_log_interface(&log_mock);
    config = rof_safety_default_config();
    TEST_ASSERT_EQUAL(ROF_OK, rof_safety_init(&safety, &config, &clk_if, &logger_if));
}

void tearDown(void) {}

/* ---- Konfiguration ------------------------------------------------------ */

/* Standardwerte aus E-016 */
static void test_default_config_has_agreed_values(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -20.0f, config.plausible_min_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 125.0f, config.plausible_max_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, config.setpoint_min_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 110.0f, config.setpoint_max_c);
    TEST_ASSERT_EQUAL_UINT32(10u * 60u * 1000u, config.freeze_timeout_ms);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.05f, config.freeze_min_change_c);
    TEST_ASSERT_EQUAL_UINT32(3000u, config.cycle_window_ms);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 120.0f, ROF_SAFETY_HARD_MAX_C);
}

/* E-007: Interfaces werden bei der Initialisierung geprüft */
static void test_init_rejects_missing_interfaces(void)
{
    rof_safety_t other;
    rof_clock_t no_time = clk_if;
    no_time.now_ms = NULL;
    rof_log_t no_write = logger_if;
    no_write.write = NULL;

    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_safety_init(NULL, &config, &clk_if, &logger_if));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_safety_init(&other, NULL, &clk_if, &logger_if));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_safety_init(&other, &config, NULL, &logger_if));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_safety_init(&other, &config, &no_time, &logger_if));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_safety_init(&other, &config, &clk_if, NULL));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_safety_init(&other, &config, &clk_if, &no_write));
}

/* Hilfsfunktion: init mit einer veränderten Kopie der Standardwerte */
static rof_status_t init_with(rof_safety_config_t changed)
{
    rof_safety_t other;
    return rof_safety_init(&other, &changed, &clk_if, &logger_if);
}

/* SAF-04, SAF-05, SAF-08, SAF-09: unsinnige Grenzen werden abgelehnt */
static void test_init_rejects_inconsistent_config(void)
{
    rof_safety_config_t c;

    c = config;
    c.plausible_min_c = c.plausible_max_c;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, init_with(c));

    c = config;
    c.setpoint_min_c = c.setpoint_max_c;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, init_with(c));

    c = config;
    c.setpoint_min_c = c.plausible_min_c - 1.0f; /* Sollwert unterhalb des plausiblen Bereichs */
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, init_with(c));

    c = config;
    c.plausible_max_c = 100.0f; /* Sollwert oberhalb des plausiblen Bereichs */
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, init_with(c));

    c = config;
    c.plausible_max_c = 130.0f;
    c.setpoint_max_c = ROF_SAFETY_HARD_MAX_C; /* muss echt unter der harten Grenze liegen */
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, init_with(c));

    c = config;
    c.freeze_timeout_ms = 0;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, init_with(c));

    c = config;
    c.freeze_min_change_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, init_with(c));

    c = config;
    c.cycle_window_ms = 0;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, init_with(c));

    c = config;
    c.plausible_min_c = NAN;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, init_with(c));
}

/* Gegenprobe: eine andere, aber stimmige Konfiguration wird angenommen */
static void test_init_accepts_other_consistent_config(void)
{
    rof_safety_config_t c = config;
    c.setpoint_min_c = 30.0f;
    c.setpoint_max_c = 90.0f;
    c.freeze_timeout_ms = 1;

    TEST_ASSERT_EQUAL(ROF_OK, init_with(c));
}

/* ---- Sollwerte (SAF-09) ------------------------------------------------- */

/* SAF-09: Grenzen selbst sind erlaubt */
static void test_setpoint_within_limits_is_ok(void)
{
    TEST_ASSERT_EQUAL(ROF_SETPOINT_OK, rof_safety_check_setpoint(&safety, 20.0f));
    TEST_ASSERT_EQUAL(ROF_SETPOINT_OK, rof_safety_check_setpoint(&safety, 75.0f));
    TEST_ASSERT_EQUAL(ROF_SETPOINT_OK, rof_safety_check_setpoint(&safety, 110.0f));
}

/* SAF-09: abgelehnt mit Grund, nicht begrenzt */
static void test_setpoint_outside_limits_is_rejected_with_reason(void)
{
    TEST_ASSERT_EQUAL(ROF_SETPOINT_TOO_LOW, rof_safety_check_setpoint(&safety, 19.99f));
    TEST_ASSERT_EQUAL(ROF_SETPOINT_TOO_LOW, rof_safety_check_setpoint(&safety, -INFINITY));
    TEST_ASSERT_EQUAL(ROF_SETPOINT_TOO_HIGH, rof_safety_check_setpoint(&safety, 110.01f));
    TEST_ASSERT_EQUAL(ROF_SETPOINT_TOO_HIGH, rof_safety_check_setpoint(&safety, INFINITY));
    TEST_ASSERT_EQUAL(ROF_SETPOINT_NOT_A_NUMBER, rof_safety_check_setpoint(&safety, NAN));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_default_config_has_agreed_values);
    RUN_TEST(test_init_rejects_missing_interfaces);
    RUN_TEST(test_init_rejects_inconsistent_config);
    RUN_TEST(test_init_accepts_other_consistent_config);

    RUN_TEST(test_setpoint_within_limits_is_ok);
    RUN_TEST(test_setpoint_outside_limits_is_rejected_with_reason);

    return UNITY_END();
}
