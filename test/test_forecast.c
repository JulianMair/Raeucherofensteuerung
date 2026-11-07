/*
 * Tests der Fertigzeit-Prognose (FC-01, E-047). Die Kurven sind synthetisch:
 * Ein Kern nähert sich dem Garraum mit bekannter Zeitkonstante tau, die
 * Prognose muss tau und die Restzeit daraus wiederfinden.
 */
#include "unity.h"

#include "rof_forecast.h"

#include <math.h>

static rof_forecast_t fc;

void setUp(void)
{
    rof_forecast_init(&fc);
}

void tearDown(void) {}

/* Kern nach t Sekunden: T_g + (T_0 - T_g) * e^(-t / tau) */
static float probe_at(float chamber_c, float start_c, float tau_s, float t_s)
{
    return chamber_c + (start_c - chamber_c) * expf(-t_s / tau_s);
}

/* Auf die Auflösung des DS18B20 gerundet, 1/16 °C */
static float quantized(float temp_c)
{
    return roundf(temp_c * 16.0f) / 16.0f;
}

/* n Punkte im Abstand von ROF_FORECAST_SAMPLE_MS, ab Zeitpunkt start_s */
static void add_curve(size_t n, float chamber_c, float start_c, float tau_s, float start_s, bool quantize)
{
    for (size_t i = 0; i < n; i++) {
        const float t_s = start_s + (float)i * (ROF_FORECAST_SAMPLE_MS / 1000.0f);
        const float probe = probe_at(chamber_c, start_c, tau_s, t_s);
        rof_forecast_add(&fc, (uint64_t)(t_s * 1000.0f), chamber_c, quantize ? quantized(probe) : probe);
    }
}

static float estimated_tau(void)
{
    float tau_s = 0.0f;
    TEST_ASSERT_TRUE(rof_forecast_time_constant(&fc, &tau_s));
    return tau_s;
}

/* ---- Zeitkonstante ------------------------------------------------------- */

static void test_exact_curve_gives_its_time_constant(void)
{
    add_curve(ROF_FORECAST_MIN_SAMPLES, 80.0f, 20.0f, 3600.0f, 0.0f, false);
    TEST_ASSERT_FLOAT_WITHIN(36.0f, 3600.0f, estimated_tau()); /* 1 Prozent */
}

static void test_shorter_time_constant(void)
{
    add_curve(ROF_FORECAST_WINDOW, 110.0f, 40.0f, 1200.0f, 0.0f, false);
    TEST_ASSERT_FLOAT_WITHIN(12.0f, 1200.0f, estimated_tau());
}

/* Später im Garen ist der Abstand klein, das Verhältnis bleibt gleich */
static void test_late_in_the_curve(void)
{
    add_curve(ROF_FORECAST_WINDOW, 80.0f, 20.0f, 3600.0f, 5400.0f, false);
    TEST_ASSERT_FLOAT_WITHIN(36.0f, 3600.0f, estimated_tau());
}

/* Echte Werte kommen in Stufen von 1/16 °C */
static void test_quantized_values_still_give_a_close_estimate(void)
{
    add_curve(ROF_FORECAST_WINDOW, 80.0f, 20.0f, 3600.0f, 600.0f, true);
    TEST_ASSERT_FLOAT_WITHIN(180.0f, 3600.0f, estimated_tau()); /* 5 Prozent */
}

/* Der Garraum schwankt mit der PWM; gerechnet wird mit seinem Mittelwert */
static void test_ripple_of_the_chamber_averages_out(void)
{
    for (size_t i = 0; i < ROF_FORECAST_WINDOW; i++) {
        const float t_s = (float)i * 10.0f;
        const float ripple = (i % 2 == 0) ? 0.2f : -0.2f;
        rof_forecast_add(&fc, (uint64_t)(t_s * 1000.0f), 80.0f + ripple, probe_at(80.0f, 20.0f, 3600.0f, t_s));
    }
    TEST_ASSERT_FLOAT_WITHIN(72.0f, 3600.0f, estimated_tau()); /* 2 Prozent */
}

/* ---- Punkte sammeln ------------------------------------------------------ */

static void test_samples_closer_than_the_interval_are_skipped(void)
{
    TEST_ASSERT_TRUE(rof_forecast_add(&fc, 0, 80.0f, 20.0f));
    TEST_ASSERT_FALSE(rof_forecast_add(&fc, ROF_FORECAST_SAMPLE_MS - 1, 80.0f, 21.0f));
    TEST_ASSERT_TRUE(rof_forecast_add(&fc, ROF_FORECAST_SAMPLE_MS, 80.0f, 21.0f));
    TEST_ASSERT_EQUAL_size_t(2, fc.count);
}

/* Das Fenster gleitet: Es zählen nur die letzten ROF_FORECAST_WINDOW Punkte */
static void test_window_keeps_only_the_latest_samples(void)
{
    /* Erst eine Kurve mit anderer Zeitkonstante, dann ein volles Fenster der richtigen */
    add_curve(ROF_FORECAST_WINDOW, 80.0f, 20.0f, 600.0f, 0.0f, false);
    add_curve(ROF_FORECAST_WINDOW, 80.0f, 30.0f, 3600.0f, 1000.0f, false);

    TEST_ASSERT_EQUAL_size_t(ROF_FORECAST_WINDOW, fc.count);
    TEST_ASSERT_FLOAT_WITHIN(36.0f, 3600.0f, estimated_tau());
}

static void test_clear_forgets_everything(void)
{
    add_curve(10, 80.0f, 20.0f, 3600.0f, 0.0f, false);
    rof_forecast_clear(&fc);

    TEST_ASSERT_EQUAL_size_t(0, fc.count);
    float tau_s = 0.0f;
    TEST_ASSERT_FALSE(rof_forecast_time_constant(&fc, &tau_s));
    TEST_ASSERT_TRUE(rof_forecast_add(&fc, 5, 80.0f, 20.0f)); /* gleich wieder ein Punkt */
}

/* ---- Keine Schätzung ----------------------------------------------------- */

static void test_one_sample_is_not_enough(void)
{
    add_curve(1, 80.0f, 20.0f, 3600.0f, 0.0f, false);
    float tau_s = 42.0f;
    TEST_ASSERT_FALSE(rof_forecast_time_constant(&fc, &tau_s));
    TEST_ASSERT_EQUAL_FLOAT(42.0f, tau_s);
}

/* Steht der Kern still oder kühlt er ab, nähert er sich nicht an */
static void test_no_approach_gives_no_estimate(void)
{
    for (size_t i = 0; i < 30; i++) {
        rof_forecast_add(&fc, i * ROF_FORECAST_SAMPLE_MS, 80.0f, 50.0f);
    }
    float tau_s = 0.0f;
    TEST_ASSERT_FALSE(rof_forecast_time_constant(&fc, &tau_s));

    rof_forecast_clear(&fc);
    for (size_t i = 0; i < 30; i++) {
        rof_forecast_add(&fc, i * ROF_FORECAST_SAMPLE_MS, 80.0f, 50.0f - (float)i * 0.1f);
    }
    TEST_ASSERT_FALSE(rof_forecast_time_constant(&fc, &tau_s));
}

/* Liegt der Kern über dem Garraum, gibt es keinen Logarithmus */
static void test_probe_above_chamber_gives_no_estimate(void)
{
    for (size_t i = 0; i < 30; i++) {
        rof_forecast_add(&fc, i * ROF_FORECAST_SAMPLE_MS, 60.0f, 61.0f + (float)i * 0.01f);
    }
    float tau_s = 0.0f;
    TEST_ASSERT_FALSE(rof_forecast_time_constant(&fc, &tau_s));
}

/* ---- Status und Restzeit ------------------------------------------------ */

/* Ein Zyklus wie im Regeltask: HALTEN, beide Fühler gültig, Ziel gesetzt */
static rof_forecast_input_t holding_input(float chamber_c, float probe_c, float target_c)
{
    return (rof_forecast_input_t){ .holding = true,
                                   .chamber_ok = true,
                                   .chamber_c = chamber_c,
                                   .probe_ok = true,
                                   .probe_c = probe_c,
                                   .has_target = true,
                                   .target_c = target_c };
}

/* Füttert n Zyklen einer Kurve im Abstand von 10 s und liefert das letzte Ergebnis */
static rof_forecast_result_t run_curve(size_t n, float chamber_c, float tau_s, float target_c, float *last_probe)
{
    rof_forecast_result_t result = { 0 };
    for (size_t i = 0; i < n; i++) {
        const float t_s = (float)i * 10.0f;
        *last_probe = probe_at(chamber_c, 20.0f, tau_s, t_s);
        const rof_forecast_input_t in = holding_input(chamber_c, *last_probe, target_c);
        result = rof_forecast_update(&fc, &in, (uint64_t)(t_s * 1000.0f));
    }
    return result;
}

/* FC-01: Restzeit nach dem Modell, tau * ln((T_g - T_k) / (T_g - T_ziel)) */
static void test_remaining_time_follows_the_model(void)
{
    float probe = 0.0f;
    const rof_forecast_result_t result = run_curve(ROF_FORECAST_MIN_SAMPLES, 80.0f, 3600.0f, 70.0f, &probe);

    TEST_ASSERT_EQUAL(ROF_FORECAST_OK, result.status);
    const float expected_s = 3600.0f * logf((80.0f - probe) / (80.0f - 70.0f));
    TEST_ASSERT_FLOAT_WITHIN(expected_s * 0.02f, expected_s, (float)result.remaining_s);
}

/* Ein Punkt weniger als nötig: lernt noch, ohne Restzeit; mit dem nächsten Punkt gibt es eine */
static void test_learning_until_enough_samples(void)
{
    float probe = 0.0f;
    const rof_forecast_result_t learning = run_curve(ROF_FORECAST_MIN_SAMPLES - 1, 80.0f, 3600.0f, 70.0f, &probe);
    TEST_ASSERT_EQUAL(ROF_FORECAST_LEARNING, learning.status);
    TEST_ASSERT_EQUAL_UINT32(0, learning.remaining_s);

    const float t_s = (float)(ROF_FORECAST_MIN_SAMPLES - 1) * 10.0f;
    const rof_forecast_input_t in = holding_input(80.0f, probe_at(80.0f, 20.0f, 3600.0f, t_s), 70.0f);
    TEST_ASSERT_EQUAL(ROF_FORECAST_OK, rof_forecast_update(&fc, &in, (uint64_t)(t_s * 1000.0f)).status);
}

static void test_reached_when_probe_is_at_target(void)
{
    rof_forecast_input_t in = holding_input(80.0f, 68.0f, 68.0f);
    TEST_ASSERT_EQUAL(ROF_FORECAST_REACHED, rof_forecast_update(&fc, &in, 0).status);

    /* auch außerhalb von HALTEN, z. B. beim Abkühlen */
    in.holding = false;
    in.probe_c = 69.0f;
    TEST_ASSERT_EQUAL(ROF_FORECAST_REACHED, rof_forecast_update(&fc, &in, 10000).status);
}

/* Ohne Ziel gibt es keine Restzeit, gelernt wird trotzdem */
static void test_no_target_still_learns(void)
{
    for (size_t i = 0; i < 5; i++) {
        rof_forecast_input_t in = holding_input(80.0f, 30.0f + (float)i, 0.0f);
        in.has_target = false;
        TEST_ASSERT_EQUAL(ROF_FORECAST_NO_TARGET, rof_forecast_update(&fc, &in, i * 10000u).status);
    }
    TEST_ASSERT_EQUAL_size_t(5, fc.count);
}

/* Außerhalb von HALTEN steigt der Garraum noch, das Modell passt nicht */
static void test_waiting_outside_holding_and_learning_starts_over(void)
{
    float probe = 0.0f;
    run_curve(ROF_FORECAST_MIN_SAMPLES, 80.0f, 3600.0f, 70.0f, &probe);

    rof_forecast_input_t in = holding_input(75.0f, probe, 70.0f);
    in.holding = false;
    TEST_ASSERT_EQUAL(ROF_FORECAST_WAITING, rof_forecast_update(&fc, &in, 400000).status);
    TEST_ASSERT_EQUAL_size_t(0, fc.count);
}

/* Fällt ein Fühler aus, beginnt das Lernen von vorn */
static void test_unavailable_without_valid_sensor_values(void)
{
    float probe = 0.0f;
    run_curve(ROF_FORECAST_MIN_SAMPLES, 80.0f, 3600.0f, 70.0f, &probe);

    rof_forecast_input_t in = holding_input(80.0f, probe, 70.0f);
    in.probe_ok = false;
    TEST_ASSERT_EQUAL(ROF_FORECAST_UNAVAILABLE, rof_forecast_update(&fc, &in, 400000).status);
    TEST_ASSERT_EQUAL_size_t(0, fc.count);

    in = holding_input(80.0f, probe, 70.0f);
    in.chamber_ok = false;
    TEST_ASSERT_EQUAL(ROF_FORECAST_UNAVAILABLE, rof_forecast_update(&fc, &in, 410000).status);

    in = holding_input(80.0f, probe, 70.0f);
    TEST_ASSERT_EQUAL(ROF_FORECAST_LEARNING, rof_forecast_update(&fc, &in, 420000).status);
}

/* Ein Ziel am oder über dem Garraum wird nie erreicht */
static void test_unreachable_when_target_is_not_below_chamber(void)
{
    float probe = 0.0f;
    TEST_ASSERT_EQUAL(ROF_FORECAST_UNREACHABLE,
                      run_curve(ROF_FORECAST_MIN_SAMPLES, 80.0f, 3600.0f, 79.6f, &probe).status);

    rof_forecast_clear(&fc);
    TEST_ASSERT_EQUAL(ROF_FORECAST_OK,
                      run_curve(ROF_FORECAST_MIN_SAMPLES, 80.0f, 3600.0f, 80.0f - ROF_FORECAST_MIN_GAP_C, &probe).status);
}

static void test_unreachable_without_approach(void)
{
    rof_forecast_result_t result = { 0 };
    for (size_t i = 0; i < ROF_FORECAST_MIN_SAMPLES; i++) {
        const rof_forecast_input_t in = holding_input(80.0f, 50.0f, 70.0f);
        result = rof_forecast_update(&fc, &in, i * 10000u);
    }
    TEST_ASSERT_EQUAL(ROF_FORECAST_UNREACHABLE, result.status);
}

/* Über 24 Stunden gilt als nicht erreichbar */
static void test_unreachable_beyond_a_day(void)
{
    float probe = 0.0f;
    TEST_ASSERT_EQUAL(ROF_FORECAST_UNREACHABLE,
                      run_curve(ROF_FORECAST_MIN_SAMPLES, 80.0f, 40000.0f, 79.0f, &probe).status);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_exact_curve_gives_its_time_constant);
    RUN_TEST(test_shorter_time_constant);
    RUN_TEST(test_late_in_the_curve);
    RUN_TEST(test_quantized_values_still_give_a_close_estimate);
    RUN_TEST(test_ripple_of_the_chamber_averages_out);
    RUN_TEST(test_samples_closer_than_the_interval_are_skipped);
    RUN_TEST(test_window_keeps_only_the_latest_samples);
    RUN_TEST(test_clear_forgets_everything);
    RUN_TEST(test_one_sample_is_not_enough);
    RUN_TEST(test_no_approach_gives_no_estimate);
    RUN_TEST(test_probe_above_chamber_gives_no_estimate);
    RUN_TEST(test_remaining_time_follows_the_model);
    RUN_TEST(test_learning_until_enough_samples);
    RUN_TEST(test_reached_when_probe_is_at_target);
    RUN_TEST(test_no_target_still_learns);
    RUN_TEST(test_waiting_outside_holding_and_learning_starts_over);
    RUN_TEST(test_unavailable_without_valid_sensor_values);
    RUN_TEST(test_unreachable_when_target_is_not_below_chamber);
    RUN_TEST(test_unreachable_without_approach);
    RUN_TEST(test_unreachable_beyond_a_day);
    return UNITY_END();
}
