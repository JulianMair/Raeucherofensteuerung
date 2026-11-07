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
    return UNITY_END();
}
