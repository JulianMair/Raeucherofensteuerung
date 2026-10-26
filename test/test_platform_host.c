/*
 * Tests der Mocks und der Fake-Uhr aus platform/host.
 * Später verlassen sich alle core-Tests auf diese Bausteine. Deshalb
 * werden sie selbst getestet. Alle Aufrufe laufen über die Interfaces
 * aus core/include, genau so, wie core sie benutzt (ARC-02).
 */
#include "unity.h"

#include "fake_clock.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- Fake-Uhr ---------------------------------------------------------- */

static void test_fake_clock_starts_at_given_time(void)
{
    fake_clock_t fake;
    fake_clock_init(&fake, 1000);
    rof_clock_t clk = fake_clock_interface(&fake);

    TEST_ASSERT_EQUAL_UINT64(1000, rof_clock_now_ms(&clk));
}

static void test_fake_clock_stands_still_until_advanced(void)
{
    fake_clock_t fake;
    fake_clock_init(&fake, 0);
    rof_clock_t clk = fake_clock_interface(&fake);

    TEST_ASSERT_EQUAL_UINT64(0, rof_clock_now_ms(&clk));
    TEST_ASSERT_EQUAL_UINT64(0, rof_clock_now_ms(&clk));

    fake_clock_advance(&fake, 250);
    TEST_ASSERT_EQUAL_UINT64(250, rof_clock_now_ms(&clk));
}

static void test_fake_clock_handles_many_simulated_hours(void)
{
    const uint64_t ten_hours_ms = 10ULL * 60 * 60 * 1000;
    fake_clock_t fake;
    fake_clock_init(&fake, 0);
    rof_clock_t clk = fake_clock_interface(&fake);

    for (int i = 0; i < 10; i++) {
        fake_clock_advance(&fake, ten_hours_ms / 10);
    }
    TEST_ASSERT_EQUAL_UINT64(ten_hours_ms, rof_clock_now_ms(&clk));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_fake_clock_starts_at_given_time);
    RUN_TEST(test_fake_clock_stands_still_until_advanced);
    RUN_TEST(test_fake_clock_handles_many_simulated_hours);

    return UNITY_END();
}
