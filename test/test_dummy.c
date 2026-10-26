/*
 * Dummy-Test aus M0. Er prüft nur, dass CMake, Unity und ctest
 * zusammenspielen. Er kann entfallen, sobald echte Tests existieren.
 */
#include "unity.h"

/* Unity ruft setUp vor und tearDown nach jedem Test auf. */
void setUp(void) {}
void tearDown(void) {}

static void test_build_chain_runs(void)
{
    TEST_ASSERT_EQUAL_INT(4, 2 + 2);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_build_chain_runs);
    return UNITY_END();
}
