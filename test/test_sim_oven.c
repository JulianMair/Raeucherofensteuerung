/*
 * Tests des Simulators (CTL-01). Geprüft wird gegen die mathematische
 * Lösung des Modells erster Ordnung mit Totzeit:
 *   T(t) = T_umgebung + K * u * (1 - exp(-(t - Totzeit) / tau))
 */
#include "unity.h"

#include "sim_oven.h"

#include <math.h>

static sim_oven_config_t config;
static sim_oven_t sim;
static rof_temp_sensor_t sensors;
static rof_output_t heater;
static rof_clock_t clk_if;

void setUp(void)
{
    config = sim_oven_default_config();
    TEST_ASSERT_EQUAL(ROF_OK, sim_oven_init(&sim, &config));
    sensors = sim_oven_sensors(&sim);
    heater = sim_oven_output(&sim);
    clk_if = sim_oven_clock(&sim);
}

void tearDown(void) {}

static float read_c(rof_sensor_id_t id)
{
    float temp_c = -999.0f;
    TEST_ASSERT_EQUAL(ROF_OK, rof_temp_sensor_read(&sensors, id, &temp_c));
    return temp_c;
}

static void run_s(unsigned seconds)
{
    sim_oven_advance(&sim, (uint64_t)seconds * 1000u);
}

/* Analytische Lösung für den Garraum bei Dauerheizen ab t = 0 */
static double chamber_expected_c(double t_s)
{
    if (t_s <= config.chamber_dead_s) {
        return config.ambient_c;
    }
    return config.ambient_c +
           config.heater_gain_c * (1.0 - exp(-(t_s - config.chamber_dead_s) / config.chamber_tau_s));
}

/* CTL-01: vereinbarte Parameter (E-025) */
static void test_default_config_has_agreed_values(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, config.ambient_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 130.0f, config.heater_gain_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 600.0f, config.chamber_tau_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, config.chamber_dead_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3600.0f, config.probe_tau_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 120.0f, config.probe_dead_s);
    TEST_ASSERT_EQUAL_UINT32(4000u, config.pwm_period_ms);
}

/* CTL-01: Parameter sind konfigurierbar, unsinnige werden abgelehnt */
static void test_init_rejects_bad_config(void)
{
    sim_oven_t other;
    sim_oven_config_t c;

    c = config;
    c.chamber_tau_s = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, sim_oven_init(&other, &c));

    c = config;
    c.probe_tau_s = -1.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, sim_oven_init(&other, &c));

    c = config;
    c.chamber_dead_s = -1.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, sim_oven_init(&other, &c));

    c = config;
    c.probe_dead_s = 100000.0f; /* länger als der Puffer für die Totzeit */
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, sim_oven_init(&other, &c));

    c = config;
    c.pwm_period_ms = 0;
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, sim_oven_init(&other, &c));
}

/* Ohne Heizen bleibt alles auf Umgebungstemperatur */
static void test_stays_at_ambient_without_heating(void)
{
    run_s(3600);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f, read_c(SIM_OVEN_CHAMBER_ID));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f, read_c(SIM_OVEN_PROBE_ID));
}

/* CTL-01: Totzeit. Während der ersten 30 s reagiert der Garraum nicht. */
static void test_chamber_does_not_react_within_dead_time(void)
{
    rof_output_set_duty(&heater, 1.0f);

    run_s(29);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f, read_c(SIM_OVEN_CHAMBER_ID));
}

/* CTL-01: Sprungantwort folgt der analytischen Lösung, z. B. nach Totzeit + tau */
static void test_step_response_matches_first_order_model(void)
{
    rof_output_set_duty(&heater, 1.0f);

    run_s(630);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, (float)chamber_expected_c(630.0), read_c(SIM_OVEN_CHAMBER_ID));

    run_s(1200);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, (float)chamber_expected_c(1830.0), read_c(SIM_OVEN_CHAMBER_ID));

    run_s(6 * 600);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 150.0f, read_c(SIM_OVEN_CHAMBER_ID));
}

/* CTL-01, CTL-03: die PWM wirkt im Mittel wie ihr Tastgrad: 50 % -> 20 + 65 °C */
static void test_half_duty_settles_at_half_heater_gain(void)
{
    rof_output_set_duty(&heater, 0.5f);

    run_s(10 * 600);

    TEST_ASSERT_FLOAT_WITHIN(1.0f, 85.0f, read_c(SIM_OVEN_CHAMBER_ID));
}

/* CTL-01: abschalten lässt den Garraum Richtung Umgebung abkühlen */
static void test_chamber_cools_down_when_heater_is_off(void)
{
    rof_output_set_duty(&heater, 1.0f);
    run_s(3600);
    const float hot_c = read_c(SIM_OVEN_CHAMBER_ID);

    rof_output_set_duty(&heater, 0.0f);
    run_s(30 + 600);

    /* Nach Totzeit + tau ist der Abstand zur Umgebung auf etwa 37 % gefallen */
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 20.0f + (hot_c - 20.0f) * expf(-1.0f), read_c(SIM_OVEN_CHAMBER_ID));
}

/* CTL-01: der Kern folgt dem Garraum langsamer und mit eigener Totzeit */
static void test_probe_follows_chamber_slowly(void)
{
    rof_output_set_duty(&heater, 1.0f);

    run_s(140); /* Garraum warm, aber Totzeit von Garraum (30 s) + Kern (120 s) noch nicht vorbei */
    TEST_ASSERT_TRUE(read_c(SIM_OVEN_CHAMBER_ID) > 30.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 20.0f, read_c(SIM_OVEN_PROBE_ID));

    run_s(2 * 3600);
    const float chamber_c = read_c(SIM_OVEN_CHAMBER_ID);
    const float probe_c = read_c(SIM_OVEN_PROBE_ID);
    TEST_ASSERT_TRUE(probe_c > 50.0f);
    TEST_ASSERT_TRUE(probe_c < chamber_c);
}

/* Die Sensoren erscheinen über das normale Treiber-Interface */
static void test_sensors_are_enumerated(void)
{
    rof_sensor_info_t list[4];
    size_t count = 0;

    TEST_ASSERT_EQUAL(ROF_OK, rof_temp_sensor_enumerate(&sensors, list, 4, &count));
    TEST_ASSERT_EQUAL_size_t(2, count);
    TEST_ASSERT_EQUAL_UINT64(SIM_OVEN_CHAMBER_ID, list[0].id);
    TEST_ASSERT_EQUAL_UINT64(SIM_OVEN_PROBE_ID, list[1].id);
    TEST_ASSERT_EQUAL_STRING("SIM", list[0].type);
}

/* Der Simulator ist zugleich die Uhr: Simulierte Zeit läuft nur mit advance() */
static void test_clock_follows_simulated_time(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, rof_clock_now_ms(&clk_if));

    run_s(90);

    TEST_ASSERT_EQUAL_UINT64(90000, rof_clock_now_ms(&clk_if));
}

/* Wie ein echter Ausgang lehnt der Simulator ungültige Tastgrade ab */
static void test_output_rejects_invalid_duty(void)
{
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_output_set_duty(&heater, 1.5f));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_output_set_duty(&heater, NAN));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, sim.duty);
}

/* Fehler einspeisen: Garraumsensor abziehen */
static void test_unplugged_chamber_sensor_disappears(void)
{
    rof_sensor_info_t list[4];
    size_t count = 0;
    float temp_c = 0.0f;

    sim.chamber_unplugged = true;

    rof_temp_sensor_enumerate(&sensors, list, 4, &count);
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_UINT64(SIM_OVEN_PROBE_ID, list[0].id);
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, rof_temp_sensor_read(&sensors, SIM_OVEN_CHAMBER_ID, &temp_c));
}

/* Fehler einspeisen: eingefrorener Wert */
static void test_frozen_chamber_sensor_keeps_reporting_old_value(void)
{
    rof_output_set_duty(&heater, 1.0f);
    run_s(600);
    sim_oven_freeze_chamber(&sim);
    const float frozen_c = read_c(SIM_OVEN_CHAMBER_ID);

    run_s(600);

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, frozen_c, read_c(SIM_OVEN_CHAMBER_ID));
    TEST_ASSERT_TRUE(sim.chamber_c > frozen_c + 10.0f);
}

/* Fehler einspeisen: Heizung klemmt auf an, egal welcher Tastgrad gesetzt ist */
static void test_stuck_heater_heats_with_duty_zero(void)
{
    sim.heater_stuck_on = true;
    rof_output_set_duty(&heater, 0.0f);

    run_s(1200);

    TEST_ASSERT_TRUE(read_c(SIM_OVEN_CHAMBER_ID) > 100.0f);
}

static void test_unknown_sensor_is_not_found(void)
{
    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, rof_temp_sensor_read(&sensors, 0x1234, &temp_c));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_default_config_has_agreed_values);
    RUN_TEST(test_init_rejects_bad_config);
    RUN_TEST(test_stays_at_ambient_without_heating);
    RUN_TEST(test_chamber_does_not_react_within_dead_time);
    RUN_TEST(test_step_response_matches_first_order_model);
    RUN_TEST(test_half_duty_settles_at_half_heater_gain);
    RUN_TEST(test_chamber_cools_down_when_heater_is_off);
    RUN_TEST(test_probe_follows_chamber_slowly);
    RUN_TEST(test_sensors_are_enumerated);
    RUN_TEST(test_clock_follows_simulated_time);
    RUN_TEST(test_output_rejects_invalid_duty);
    RUN_TEST(test_unplugged_chamber_sensor_disappears);
    RUN_TEST(test_frozen_chamber_sensor_keeps_reporting_old_value);
    RUN_TEST(test_stuck_heater_heats_with_duty_zero);
    RUN_TEST(test_unknown_sensor_is_not_found);

    return UNITY_END();
}
