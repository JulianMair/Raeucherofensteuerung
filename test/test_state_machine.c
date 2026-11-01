/*
 * Tests des Zustandsautomaten core/control (CTL-04, CTL-05, SAF-10).
 * Jeder Übergang aus docs/zustandsautomat.md hat einen Test; der Kommentar
 * nennt seine Nummer (Ü1 bis Ü10).
 */
#include "unity.h"

#include "mock_log.h"
#include "rof_state_machine.h"

static mock_log_t log_mock;
static rof_log_t logger_if;
static rof_fsm_config_t config;
static rof_fsm_t fsm;

void setUp(void)
{
    mock_log_init(&log_mock);
    logger_if = mock_log_interface(&log_mock);
    config = rof_fsm_default_config();
    TEST_ASSERT_EQUAL(ROF_OK, rof_fsm_init(&fsm, &config, &logger_if));
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

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_starts_idle_with_output_off_and_no_setpoint);
    RUN_TEST(test_default_config_has_agreed_values);
    RUN_TEST(test_init_rejects_missing_logger_and_bad_config);

    return UNITY_END();
}
