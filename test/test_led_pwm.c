/*
 * Tests der Schaltlogik des LED-Ausgangs aus platform/esp32
 * (CTL-03, SAF-08, SAF-10, E-023, E-034).
 */
#include "unity.h"

#include "led_pwm.h"

#include <math.h>

#define PERIOD_MS 4000u
#define HOLD_MS 3000u

static led_pwm_t pwm;

void setUp(void)
{
    led_pwm_init(&pwm, PERIOD_MS, HOLD_MS);
}

void tearDown(void) {}

/* SAF-10: Nach dem Start ist die LED aus, bis jemand einen Tastgrad setzt */
static void test_off_before_first_duty(void)
{
    for (uint64_t t = 0; t < 2 * PERIOD_MS; t += 50) {
        TEST_ASSERT_FALSE(led_pwm_is_on(&pwm, t));
    }
}

/* CTL-03: Die LED folgt der langsamen PWM aus core, vorne an, hinten aus */
static void test_follows_slow_pwm(void)
{
    TEST_ASSERT_EQUAL(ROF_OK, led_pwm_set_duty(&pwm, 0.25f, 4000));
    TEST_ASSERT_TRUE(led_pwm_is_on(&pwm, 4000));
    TEST_ASSERT_TRUE(led_pwm_is_on(&pwm, 4999));
    TEST_ASSERT_FALSE(led_pwm_is_on(&pwm, 5000));
    TEST_ASSERT_FALSE(led_pwm_is_on(&pwm, 6999));
}

static void test_zero_and_full_duty(void)
{
    TEST_ASSERT_EQUAL(ROF_OK, led_pwm_set_duty(&pwm, 0.0f, 1000));
    TEST_ASSERT_FALSE(led_pwm_is_on(&pwm, 1000));
    TEST_ASSERT_FALSE(led_pwm_is_on(&pwm, 2000));

    TEST_ASSERT_EQUAL(ROF_OK, led_pwm_set_duty(&pwm, 1.0f, 2000));
    TEST_ASSERT_TRUE(led_pwm_is_on(&pwm, 2000));
    TEST_ASSERT_TRUE(led_pwm_is_on(&pwm, 3999));
}

/* Ungültiger Tastgrad: abgelehnt, der vorige gilt weiter (Interface rof_output_t) */
static void test_invalid_duty_is_rejected_and_keeps_previous(void)
{
    TEST_ASSERT_EQUAL(ROF_OK, led_pwm_set_duty(&pwm, 1.0f, 0));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, led_pwm_set_duty(&pwm, 1.5f, 100));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, led_pwm_set_duty(&pwm, -0.1f, 100));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, led_pwm_set_duty(&pwm, NAN, 100));
    TEST_ASSERT_TRUE(led_pwm_is_on(&pwm, 200));
}

/* Ein abgelehnter Tastgrad frischt auch die Haltezeit nicht auf */
static void test_invalid_duty_does_not_refresh_hold(void)
{
    TEST_ASSERT_EQUAL(ROF_OK, led_pwm_set_duty(&pwm, 1.0f, 0));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, led_pwm_set_duty(&pwm, 2.0f, 2500));
    TEST_ASSERT_FALSE(led_pwm_is_on(&pwm, HOLD_MS + 1));
}

/* E-034: Ohne Auffrischen geht die LED nach der Haltezeit aus */
static void test_off_when_duty_is_not_refreshed(void)
{
    TEST_ASSERT_EQUAL(ROF_OK, led_pwm_set_duty(&pwm, 1.0f, 0));
    TEST_ASSERT_TRUE(led_pwm_is_on(&pwm, HOLD_MS));
    TEST_ASSERT_FALSE(led_pwm_is_on(&pwm, HOLD_MS + 1));
    TEST_ASSERT_FALSE(led_pwm_is_on(&pwm, 10 * HOLD_MS));
}

/* Wie im Regeltask: jede Sekunde auffrischen hält sie an */
static void test_refreshing_every_second_keeps_it_on(void)
{
    for (uint64_t t = 0; t < 20000; t += 1000) {
        TEST_ASSERT_EQUAL(ROF_OK, led_pwm_set_duty(&pwm, 1.0f, t));
        TEST_ASSERT_TRUE(led_pwm_is_on(&pwm, t + 999));
    }
}

/* Der Timer kann die Zeit kurz vor dem Setzen gelesen haben. Das ist
 * kein Ablauf der Haltezeit. */
static void test_time_slightly_before_set_counts_as_fresh(void)
{
    TEST_ASSERT_EQUAL(ROF_OK, led_pwm_set_duty(&pwm, 1.0f, 5000));
    TEST_ASSERT_TRUE(led_pwm_is_on(&pwm, 4990));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_off_before_first_duty);
    RUN_TEST(test_follows_slow_pwm);
    RUN_TEST(test_zero_and_full_duty);
    RUN_TEST(test_invalid_duty_is_rejected_and_keeps_previous);
    RUN_TEST(test_invalid_duty_does_not_refresh_hold);
    RUN_TEST(test_off_when_duty_is_not_refreshed);
    RUN_TEST(test_refreshing_every_second_keeps_it_on);
    RUN_TEST(test_time_slightly_before_set_counts_as_fresh);
    return UNITY_END();
}
