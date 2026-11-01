/*
 * Tests der langsamen PWM (CTL-03).
 */
#include "unity.h"

#include "rof_slow_pwm.h"

#include <math.h>

void setUp(void) {}
void tearDown(void) {}

/* Wie viele Millisekunden eines Fensters ist der Ausgang an? */
static unsigned on_ms_in_window(uint32_t period_ms, float duty, uint64_t window_start_ms)
{
    unsigned on_ms = 0;
    for (uint64_t t = window_start_ms; t < window_start_ms + period_ms; t++) {
        if (rof_slow_pwm_on(period_ms, duty, t)) {
            on_ms++;
        }
    }
    return on_ms;
}

/* CTL-03: Standardperiode 4 Sekunden */
static void test_default_period_is_four_seconds(void)
{
    TEST_ASSERT_EQUAL_UINT32(4000u, ROF_SLOW_PWM_DEFAULT_PERIOD_MS);
}

/* CTL-03: Tastgrad 0,25 heißt 1 s an, dann 3 s aus, in jedem Fenster */
static void test_quarter_duty_is_on_at_the_start_of_each_window(void)
{
    TEST_ASSERT_TRUE(rof_slow_pwm_on(4000, 0.25f, 0));
    TEST_ASSERT_TRUE(rof_slow_pwm_on(4000, 0.25f, 999));
    TEST_ASSERT_FALSE(rof_slow_pwm_on(4000, 0.25f, 1000));
    TEST_ASSERT_FALSE(rof_slow_pwm_on(4000, 0.25f, 3999));
    TEST_ASSERT_TRUE(rof_slow_pwm_on(4000, 0.25f, 4000));
    TEST_ASSERT_TRUE(rof_slow_pwm_on(4000, 0.25f, 3u * 3600u * 1000u)); /* nach 3 h */
}

/* CTL-03: über ein Fenster gemittelt ergibt sich genau der Tastgrad */
static void test_on_time_per_window_matches_duty(void)
{
    TEST_ASSERT_EQUAL_UINT(1000, on_ms_in_window(4000, 0.25f, 0));
    TEST_ASSERT_EQUAL_UINT(2000, on_ms_in_window(4000, 0.5f, 8000));
    TEST_ASSERT_EQUAL_UINT(3000, on_ms_in_window(4000, 0.75f, 40000));
}

/* CTL-03: die Periode ist konfigurierbar */
static void test_period_is_configurable(void)
{
    TEST_ASSERT_EQUAL_UINT(500, on_ms_in_window(1000, 0.5f, 0));
    TEST_ASSERT_FALSE(rof_slow_pwm_on(1000, 0.5f, 500));
}

/* Tastgrad 0 ist immer aus, 1 immer an */
static void test_zero_and_full_duty(void)
{
    TEST_ASSERT_EQUAL_UINT(0, on_ms_in_window(4000, 0.0f, 0));
    TEST_ASSERT_EQUAL_UINT(4000, on_ms_in_window(4000, 1.0f, 0));
}

/* Ungültige Werte schalten aus, nie an: NaN, negativ, Periode 0. Über 1 ist dauernd an. */
static void test_invalid_values_switch_off(void)
{
    TEST_ASSERT_FALSE(rof_slow_pwm_on(4000, NAN, 0));
    TEST_ASSERT_FALSE(rof_slow_pwm_on(4000, -0.5f, 0));
    TEST_ASSERT_FALSE(rof_slow_pwm_on(0, 0.5f, 0));
    TEST_ASSERT_TRUE(rof_slow_pwm_on(4000, 1.5f, 3999));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_default_period_is_four_seconds);
    RUN_TEST(test_quarter_duty_is_on_at_the_start_of_each_window);
    RUN_TEST(test_on_time_per_window_matches_duty);
    RUN_TEST(test_period_is_configurable);
    RUN_TEST(test_zero_and_full_duty);
    RUN_TEST(test_invalid_values_switch_off);

    return UNITY_END();
}
