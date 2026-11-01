/*
 * Tests des Zustandsautomaten core/control (CTL-04, CTL-05, SAF-10).
 * Jeder Übergang aus docs/zustandsautomat.md hat einen Test; der Kommentar
 * nennt seine Nummer (Ü1 bis Ü10).
 */
#include "unity.h"

#include "fake_clock.h"
#include "mock_log.h"
#include "rof_state_machine.h"

#include <math.h>

static mock_log_t log_mock;
static rof_log_t logger_if;
static rof_fsm_config_t config;
static rof_fsm_t fsm;

/* Nur für die Sollwertprüfung (SAF-09), die in der Sicherheitslogik liegt */
static fake_clock_t clock_fake;
static rof_clock_t clk_if;
static rof_safety_t safety;

void setUp(void)
{
    mock_log_init(&log_mock);
    logger_if = mock_log_interface(&log_mock);
    config = rof_fsm_default_config();
    TEST_ASSERT_EQUAL(ROF_OK, rof_fsm_init(&fsm, &config, &logger_if));

    fake_clock_init(&clock_fake, 0);
    clk_if = fake_clock_interface(&clock_fake);
    rof_safety_config_t safety_config = rof_safety_default_config();
    TEST_ASSERT_EQUAL(ROF_OK, rof_safety_init(&safety, &safety_config, &clk_if, &logger_if));
}

void tearDown(void) {}

/* ---- Start und Konfiguration -------------------------------------------- */

/* SAF-10: nach einem Neustart BEREIT, Ausgang aus, kein Sollwert */
static void test_starts_idle_with_output_off_and_no_setpoint(void)
{
    float setpoint_c = -1.0f;

    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, rof_fsm_state(&fsm));
    TEST_ASSERT_FALSE(rof_fsm_output_enabled(&fsm));
    TEST_ASSERT_FALSE(rof_fsm_setpoint(&fsm, &setpoint_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, setpoint_c);
}

/* Standardwerte aus E-021 */
static void test_default_config_has_agreed_values(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, config.hold_band_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, config.reheat_band_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, config.cooled_c);
}

/* E-007: Interfaces und Konfiguration werden bei der Initialisierung geprüft */
static void test_init_rejects_missing_logger_and_bad_config(void)
{
    rof_fsm_t other;
    rof_log_t no_write = logger_if;
    no_write.write = NULL;
    rof_fsm_config_t c;

    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_fsm_init(NULL, &config, &logger_if));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_fsm_init(&other, NULL, &logger_if));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_fsm_init(&other, &config, NULL));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_fsm_init(&other, &config, &no_write));

    c = config;
    c.hold_band_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_fsm_init(&other, &c, &logger_if));

    c = config;
    c.reheat_band_c = c.hold_band_c; /* Rückkehr muss tiefer liegen als das Halten */
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_fsm_init(&other, &c, &logger_if));

    c = config;
    c.cooled_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_fsm_init(&other, &c, &logger_if));
}

/* ---- Sollwert und Start ------------------------------------------------- */

#define SETPOINT_C 80.0f

static void set_valid_setpoint(void)
{
    TEST_ASSERT_EQUAL(ROF_CMD_OK, rof_fsm_set_setpoint(&fsm, &safety, SETPOINT_C));
}

/* SAF-09 */
static void test_valid_setpoint_is_stored(void)
{
    float setpoint_c = 0.0f;

    set_valid_setpoint();

    TEST_ASSERT_TRUE(rof_fsm_setpoint(&fsm, &setpoint_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, SETPOINT_C, setpoint_c);
}

/* SAF-09: abgelehnt mit Grund, der alte Sollwert bleibt */
static void test_invalid_setpoint_is_rejected_with_reason(void)
{
    float setpoint_c = 0.0f;
    set_valid_setpoint();

    TEST_ASSERT_EQUAL(ROF_CMD_SETPOINT_TOO_LOW, rof_fsm_set_setpoint(&fsm, &safety, 19.0f));
    TEST_ASSERT_EQUAL(ROF_CMD_SETPOINT_TOO_HIGH, rof_fsm_set_setpoint(&fsm, &safety, 111.0f));
    TEST_ASSERT_EQUAL(ROF_CMD_SETPOINT_NOT_A_NUMBER, rof_fsm_set_setpoint(&fsm, &safety, NAN));

    rof_fsm_setpoint(&fsm, &setpoint_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, SETPOINT_C, setpoint_c);
}

/* E-022: Start ohne Sollwert wird abgelehnt, der Zustand bleibt */
static void test_start_without_setpoint_is_rejected(void)
{
    TEST_ASSERT_EQUAL(ROF_CMD_NO_SETPOINT, rof_fsm_start(&fsm));
    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, rof_fsm_state(&fsm));
}

/* Ü1: BEREIT + Start mit Sollwert -> AUFHEIZEN */
static void test_u1_start_in_idle_begins_heating(void)
{
    set_valid_setpoint();

    TEST_ASSERT_EQUAL(ROF_CMD_OK, rof_fsm_start(&fsm));

    TEST_ASSERT_EQUAL(ROF_STATE_HEATING, rof_fsm_state(&fsm));
    TEST_ASSERT_TRUE(rof_fsm_output_enabled(&fsm));
    TEST_ASSERT_EQUAL_UINT(1, log_mock.count[ROF_LOG_INFO]);
}

/* Start während schon geheizt wird, ändert nichts */
static void test_start_while_heating_is_rejected(void)
{
    set_valid_setpoint();
    rof_fsm_start(&fsm);

    TEST_ASSERT_EQUAL(ROF_CMD_ALREADY_RUNNING, rof_fsm_start(&fsm));
    TEST_ASSERT_EQUAL(ROF_STATE_HEATING, rof_fsm_state(&fsm));
}

/* ---- Aufheizen und Halten ----------------------------------------------- */

/* Ein Regelzyklus ohne Fehler mit diesem Garraumwert */
static void step_ok(float chamber_c)
{
    rof_fsm_input_t in = {
        .latched = ROF_FAULT_NONE,
        .active = ROF_FAULT_NONE,
        .chamber_temp_c = chamber_c,
    };
    rof_fsm_step(&fsm, &in);
}

static void go_heating(void)
{
    set_valid_setpoint();
    TEST_ASSERT_EQUAL(ROF_CMD_OK, rof_fsm_start(&fsm));
}

static void go_holding(void)
{
    go_heating();
    step_ok(SETPOINT_C - 2.0f);
    TEST_ASSERT_EQUAL(ROF_STATE_HOLDING, rof_fsm_state(&fsm));
}

/* Nichtübergang zu Ü2: knapp unter Sollwert - 2 °C bleibt AUFHEIZEN */
static void test_heating_stays_below_hold_band(void)
{
    go_heating();

    step_ok(SETPOINT_C - 2.1f);

    TEST_ASSERT_EQUAL(ROF_STATE_HEATING, rof_fsm_state(&fsm));
}

/* Ü2: AUFHEIZEN + Garraum >= Sollwert - 2 °C -> HALTEN */
static void test_u2_heating_reaches_hold_band(void)
{
    go_heating();

    step_ok(SETPOINT_C - 2.0f);

    TEST_ASSERT_EQUAL(ROF_STATE_HOLDING, rof_fsm_state(&fsm));
    TEST_ASSERT_TRUE(rof_fsm_output_enabled(&fsm));
}

/* Nichtübergang zu Ü3: genau Sollwert - 10 °C bleibt HALTEN */
static void test_holding_stays_at_reheat_band(void)
{
    go_holding();

    step_ok(SETPOINT_C - 10.0f);

    TEST_ASSERT_EQUAL(ROF_STATE_HOLDING, rof_fsm_state(&fsm));
}

/* Ü3: HALTEN + Garraum < Sollwert - 10 °C -> AUFHEIZEN, z. B. Tür offen */
static void test_u3_holding_falls_below_reheat_band(void)
{
    go_holding();

    step_ok(SETPOINT_C - 10.1f);

    TEST_ASSERT_EQUAL(ROF_STATE_HEATING, rof_fsm_state(&fsm));
}

/* Ü3: ein deutlich erhöhter Sollwert führt ebenfalls zurück nach AUFHEIZEN */
static void test_u3_raised_setpoint_returns_to_heating(void)
{
    go_holding();

    TEST_ASSERT_EQUAL(ROF_CMD_OK, rof_fsm_set_setpoint(&fsm, &safety, SETPOINT_C + 20.0f));
    step_ok(SETPOINT_C - 2.0f);

    TEST_ASSERT_EQUAL(ROF_STATE_HEATING, rof_fsm_state(&fsm));
}

/* BEREIT hat ohne Befehl keinen Übergang, egal wie warm es ist */
static void test_idle_stays_idle_without_command(void)
{
    set_valid_setpoint();

    step_ok(SETPOINT_C);

    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, rof_fsm_state(&fsm));
}

/* ---- Stopp und Abkühlen ------------------------------------------------- */

static void go_cooling(void)
{
    go_heating();
    TEST_ASSERT_EQUAL(ROF_CMD_OK, rof_fsm_stop(&fsm));
    TEST_ASSERT_EQUAL(ROF_STATE_COOLING, rof_fsm_state(&fsm));
}

/* Ü4: AUFHEIZEN + Stopp -> ABKUEHLEN, Ausgang aus */
static void test_u4_stop_while_heating_cools_down(void)
{
    go_heating();

    TEST_ASSERT_EQUAL(ROF_CMD_OK, rof_fsm_stop(&fsm));

    TEST_ASSERT_EQUAL(ROF_STATE_COOLING, rof_fsm_state(&fsm));
    TEST_ASSERT_FALSE(rof_fsm_output_enabled(&fsm));
}

/* Ü4: HALTEN + Stopp -> ABKUEHLEN */
static void test_u4_stop_while_holding_cools_down(void)
{
    go_holding();

    TEST_ASSERT_EQUAL(ROF_CMD_OK, rof_fsm_stop(&fsm));

    TEST_ASSERT_EQUAL(ROF_STATE_COOLING, rof_fsm_state(&fsm));
}

/* Nichtübergang zu Ü5: genau 50 °C ist noch nicht abgekühlt */
static void test_cooling_stays_at_threshold(void)
{
    go_cooling();

    step_ok(50.0f);

    TEST_ASSERT_EQUAL(ROF_STATE_COOLING, rof_fsm_state(&fsm));
}

/* Ü5: ABKUEHLEN + Garraum < 50 °C -> BEREIT */
static void test_u5_cooled_down_is_idle(void)
{
    go_cooling();

    step_ok(49.9f);

    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, rof_fsm_state(&fsm));
}

/* Ü6: ABKUEHLEN + Start -> AUFHEIZEN */
static void test_u6_start_while_cooling_heats_again(void)
{
    go_cooling();

    TEST_ASSERT_EQUAL(ROF_CMD_OK, rof_fsm_start(&fsm));

    TEST_ASSERT_EQUAL(ROF_STATE_HEATING, rof_fsm_state(&fsm));
}

/* Stopp ohne Heizen tut nichts und gilt als erfolgreich */
static void test_stop_in_idle_does_nothing(void)
{
    TEST_ASSERT_EQUAL(ROF_CMD_OK, rof_fsm_stop(&fsm));

    TEST_ASSERT_EQUAL(ROF_STATE_IDLE, rof_fsm_state(&fsm));
    TEST_ASSERT_EQUAL_UINT(0, log_mock.count[ROF_LOG_INFO]);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_starts_idle_with_output_off_and_no_setpoint);
    RUN_TEST(test_default_config_has_agreed_values);
    RUN_TEST(test_init_rejects_missing_logger_and_bad_config);

    RUN_TEST(test_valid_setpoint_is_stored);
    RUN_TEST(test_invalid_setpoint_is_rejected_with_reason);
    RUN_TEST(test_start_without_setpoint_is_rejected);
    RUN_TEST(test_u1_start_in_idle_begins_heating);
    RUN_TEST(test_start_while_heating_is_rejected);

    RUN_TEST(test_heating_stays_below_hold_band);
    RUN_TEST(test_u2_heating_reaches_hold_band);
    RUN_TEST(test_holding_stays_at_reheat_band);
    RUN_TEST(test_u3_holding_falls_below_reheat_band);
    RUN_TEST(test_u3_raised_setpoint_returns_to_heating);
    RUN_TEST(test_idle_stays_idle_without_command);

    RUN_TEST(test_u4_stop_while_heating_cools_down);
    RUN_TEST(test_u4_stop_while_holding_cools_down);
    RUN_TEST(test_cooling_stays_at_threshold);
    RUN_TEST(test_u5_cooled_down_is_idle);
    RUN_TEST(test_u6_start_while_cooling_heats_again);
    RUN_TEST(test_stop_in_idle_does_nothing);

    return UNITY_END();
}
