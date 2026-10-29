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

/* ---- Auswerten und Verriegeln ------------------------------------------- */

/* Eingabe mit gültigem Garraumwert */
static rof_safety_input_t chamber_ok(float temp_c, float duty)
{
    return (rof_safety_input_t){
        .chamber_status = ROF_ROLE_STATUS_OK,
        .chamber_temp_c = temp_c,
        .applied_duty = duty,
    };
}

/* Eingabe mit Garraumstatus, aber ohne gültigen Wert */
static rof_safety_input_t chamber_with(rof_role_status_t status)
{
    return (rof_safety_input_t){ .chamber_status = status, .chamber_temp_c = 0.0f };
}

/* SAF-02: nach init ist nichts verriegelt */
static void test_new_safety_is_not_tripped(void)
{
    TEST_ASSERT_EQUAL(ROF_FAULT_NONE, rof_safety_fault(&safety));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.6f, rof_safety_limit_duty(&safety, 0.6f));
}

/* SAF-03 und SAF-07 nicht auslösen: Garraum zugeordnet und fehlerfrei gelesen */
static void test_valid_chamber_does_not_trip(void)
{
    rof_safety_input_t in = chamber_ok(80.0f, 0.5f);

    TEST_ASSERT_EQUAL(ROF_FAULT_NONE, rof_safety_evaluate(&safety, &in));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.6f, rof_safety_limit_duty(&safety, 0.6f));
    TEST_ASSERT_EQUAL_UINT(0, log_mock.count[ROF_LOG_ERROR]);
}

/* SAF-07 auslösen: Garraum keinem Sensor zugeordnet */
static void test_unassigned_chamber_trips(void)
{
    rof_safety_input_t in = chamber_with(ROF_ROLE_STATUS_UNASSIGNED);

    TEST_ASSERT_EQUAL(ROF_FAULT_CHAMBER_UNASSIGNED, rof_safety_evaluate(&safety, &in));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, rof_safety_limit_duty(&safety, 0.6f));
    TEST_ASSERT_EQUAL_UINT(1, log_mock.count[ROF_LOG_ERROR]);
}

/* SAF-07 auslösen: Garraumsensor fehlt */
static void test_missing_chamber_trips(void)
{
    rof_safety_input_t in = chamber_with(ROF_ROLE_STATUS_MISSING);

    TEST_ASSERT_EQUAL(ROF_FAULT_CHAMBER_MISSING, rof_safety_evaluate(&safety, &in));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, rof_safety_limit_duty(&safety, 1.0f));
}

/* SAF-02: die Verriegelung bleibt, auch wenn die Ursache verschwindet */
static void test_trip_stays_latched_after_cause_is_gone(void)
{
    rof_safety_input_t missing = chamber_with(ROF_ROLE_STATUS_MISSING);
    rof_safety_input_t ok = chamber_ok(80.0f, 0.5f);
    rof_safety_evaluate(&safety, &missing);

    TEST_ASSERT_EQUAL(ROF_FAULT_CHAMBER_MISSING, rof_safety_evaluate(&safety, &ok));
    TEST_ASSERT_EQUAL(ROF_FAULT_CHAMBER_MISSING, rof_safety_fault(&safety));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, rof_safety_limit_duty(&safety, 0.6f));
}

/* SAF-02: die erste Ursache bleibt stehen und wird nur einmal geloggt */
static void test_first_cause_is_kept(void)
{
    rof_safety_input_t missing = chamber_with(ROF_ROLE_STATUS_MISSING);
    rof_safety_input_t unassigned = chamber_with(ROF_ROLE_STATUS_UNASSIGNED);
    rof_safety_evaluate(&safety, &missing);

    TEST_ASSERT_EQUAL(ROF_FAULT_CHAMBER_MISSING, rof_safety_evaluate(&safety, &unassigned));
    TEST_ASSERT_EQUAL_UINT(1, log_mock.count[ROF_LOG_ERROR]);
}

/* ---- Lesefehler und doppelte ID (SAF-03) -------------------------------- */

/* SAF-03 auslösen: Lesefehler des Garraumsensors */
static void test_chamber_read_error_trips(void)
{
    rof_safety_input_t in = chamber_with(ROF_ROLE_STATUS_READ_ERROR);

    TEST_ASSERT_EQUAL(ROF_FAULT_CHAMBER_READ_ERROR, rof_safety_evaluate(&safety, &in));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, rof_safety_limit_duty(&safety, 0.6f));
}

/* SAF-03 auslösen: ID doppelt, der Wert ist nicht eindeutig (E-013) */
static void test_chamber_duplicate_trips(void)
{
    rof_safety_input_t in = chamber_with(ROF_ROLE_STATUS_DUPLICATE);

    TEST_ASSERT_EQUAL(ROF_FAULT_CHAMBER_DUPLICATE, rof_safety_evaluate(&safety, &in));
}

/* SAF-03: ein unbekannter Status gilt als Lesefehler, nicht als gültig */
static void test_unknown_chamber_status_counts_as_read_error(void)
{
    rof_safety_input_t in = chamber_with((rof_role_status_t)99);

    TEST_ASSERT_EQUAL(ROF_FAULT_CHAMBER_READ_ERROR, rof_safety_evaluate(&safety, &in));
}

/* ---- Plausibilität (SAF-04) --------------------------------------------- */

/* Frisch initialisieren und genau einen Zyklus mit diesem Garraumwert
 * auswerten. Nötig, weil jede Auslösung verriegelt. */
static rof_fault_t evaluate_fresh(float temp_c)
{
    TEST_ASSERT_EQUAL(ROF_OK, rof_safety_init(&safety, &config, &clk_if, &logger_if));
    rof_safety_input_t in = chamber_ok(temp_c, 0.0f);
    return rof_safety_evaluate(&safety, &in);
}

/* SAF-04 nicht auslösen: Werte im plausiblen Bereich, Grenzen eingeschlossen */
static void test_plausible_values_do_not_trip(void)
{
    TEST_ASSERT_EQUAL(ROF_FAULT_NONE, evaluate_fresh(-20.0f));
    TEST_ASSERT_EQUAL(ROF_FAULT_NONE, evaluate_fresh(21.5f));
    /* 125 °C ist noch plausibel. Was dort sonst auslöst, prüft SAF-06. */
    TEST_ASSERT_NOT_EQUAL(ROF_FAULT_IMPLAUSIBLE, evaluate_fresh(125.0f));
}

/* SAF-04 auslösen: außerhalb des plausiblen Bereichs oder kein Zahlenwert */
static void test_implausible_values_trip(void)
{
    TEST_ASSERT_EQUAL(ROF_FAULT_IMPLAUSIBLE, evaluate_fresh(-20.01f));
    TEST_ASSERT_EQUAL(ROF_FAULT_IMPLAUSIBLE, evaluate_fresh(125.01f));
    TEST_ASSERT_EQUAL(ROF_FAULT_IMPLAUSIBLE, evaluate_fresh(NAN));
    TEST_ASSERT_EQUAL(ROF_FAULT_IMPLAUSIBLE, evaluate_fresh(-INFINITY));
}

/* SAF-04: die Grenzen sind konfigurierbar */
static void test_plausible_range_is_configurable(void)
{
    config.plausible_min_c = 0.0f;
    config.setpoint_min_c = 20.0f;

    TEST_ASSERT_EQUAL(ROF_FAULT_IMPLAUSIBLE, evaluate_fresh(-1.0f));
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

    RUN_TEST(test_new_safety_is_not_tripped);
    RUN_TEST(test_valid_chamber_does_not_trip);
    RUN_TEST(test_unassigned_chamber_trips);
    RUN_TEST(test_missing_chamber_trips);
    RUN_TEST(test_trip_stays_latched_after_cause_is_gone);
    RUN_TEST(test_first_cause_is_kept);

    RUN_TEST(test_chamber_read_error_trips);
    RUN_TEST(test_chamber_duplicate_trips);
    RUN_TEST(test_unknown_chamber_status_counts_as_read_error);

    RUN_TEST(test_plausible_values_do_not_trip);
    RUN_TEST(test_implausible_values_trip);
    RUN_TEST(test_plausible_range_is_configurable);

    return UNITY_END();
}
