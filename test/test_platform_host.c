/*
 * Tests der Mocks und der Fake-Uhr aus platform/host.
 * Später verlassen sich alle core-Tests auf diese Bausteine. Deshalb
 * werden sie selbst getestet. Alle Aufrufe laufen über die Interfaces
 * aus core/include, genau so, wie core sie benutzt (ARC-02).
 */
#include "unity.h"

#include "fake_clock.h"
#include "mock_output.h"
#include "mock_temp_sensor.h"

#include <math.h>

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

/* ---- Mock-Temperatursensor --------------------------------------------- */

#define ID_A 0x28AA000000000001ULL
#define ID_B 0x28BB000000000002ULL

static mock_temp_sensor_t bus_mock;
static rof_temp_sensor_t bus;

static void bus_setup(void)
{
    mock_temp_sensor_init(&bus_mock);
    bus = mock_temp_sensor_interface(&bus_mock);
}

static void test_mock_sensor_empty_bus_enumerates_nothing(void)
{
    bus_setup();
    rof_sensor_info_t list[4];
    size_t count = 99;

    TEST_ASSERT_EQUAL(ROF_OK, rof_temp_sensor_enumerate(&bus, list, 4, &count));
    TEST_ASSERT_EQUAL_size_t(0, count);
}

static void test_mock_sensor_enumerates_plugged_sensors_in_order(void)
{
    bus_setup();
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    mock_temp_sensor_plug(&bus_mock, ID_B, "PT1000", 21.0f);
    rof_sensor_info_t list[4];
    size_t count = 0;

    TEST_ASSERT_EQUAL(ROF_OK, rof_temp_sensor_enumerate(&bus, list, 4, &count));
    TEST_ASSERT_EQUAL_size_t(2, count);
    TEST_ASSERT_EQUAL_UINT64(ID_A, list[0].id);
    TEST_ASSERT_EQUAL_STRING("DS18B20", list[0].type);
    TEST_ASSERT_EQUAL_UINT64(ID_B, list[1].id);
    TEST_ASSERT_EQUAL_STRING("PT1000", list[1].type);
}

static void test_mock_sensor_reads_current_temperature(void)
{
    bus_setup();
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    float temp_c = 0.0f;

    TEST_ASSERT_EQUAL(ROF_OK, rof_temp_sensor_read(&bus, ID_A, &temp_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, temp_c);

    TEST_ASSERT_EQUAL(ROF_OK, mock_temp_sensor_set_temp(&bus_mock, ID_A, 85.5f));
    TEST_ASSERT_EQUAL(ROF_OK, rof_temp_sensor_read(&bus, ID_A, &temp_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 85.5f, temp_c);
    TEST_ASSERT_EQUAL_UINT(2, bus_mock.read_calls);
}

static void test_mock_sensor_unplugged_sensor_disappears(void)
{
    bus_setup();
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    mock_temp_sensor_plug(&bus_mock, ID_B, "DS18B20", 21.0f);
    TEST_ASSERT_EQUAL(ROF_OK, mock_temp_sensor_unplug(&bus_mock, ID_A));
    rof_sensor_info_t list[4];
    size_t count = 0;
    float temp_c = -1.0f;

    TEST_ASSERT_EQUAL(ROF_OK, rof_temp_sensor_enumerate(&bus, list, 4, &count));
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_UINT64(ID_B, list[0].id);
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, rof_temp_sensor_read(&bus, ID_A, &temp_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, temp_c);
}

static void test_mock_sensor_injected_read_error_keeps_value(void)
{
    bus_setup();
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    TEST_ASSERT_EQUAL(ROF_OK, mock_temp_sensor_set_read_status(&bus_mock, ID_A, ROF_ERR_IO));
    float temp_c = -1.0f;

    TEST_ASSERT_EQUAL(ROF_ERR_IO, rof_temp_sensor_read(&bus, ID_A, &temp_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, temp_c);
}

static void test_mock_sensor_more_sensors_than_list_reports_no_space(void)
{
    bus_setup();
    for (rof_sensor_id_t id = 1; id <= 9; id++) {
        TEST_ASSERT_EQUAL(ROF_OK, mock_temp_sensor_plug(&bus_mock, id, "DS18B20", 20.0f));
    }
    rof_sensor_info_t list[8];
    size_t count = 0;

    TEST_ASSERT_EQUAL(ROF_ERR_NO_SPACE, rof_temp_sensor_enumerate(&bus, list, 8, &count));
    TEST_ASSERT_EQUAL_size_t(8, count);
    TEST_ASSERT_EQUAL_UINT64(1, list[0].id);
    TEST_ASSERT_EQUAL_UINT64(8, list[7].id);
}

static void test_mock_sensor_injected_bus_error(void)
{
    bus_setup();
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    mock_temp_sensor_set_enumerate_status(&bus_mock, ROF_ERR_IO);
    rof_sensor_info_t list[4];
    size_t count = 99;

    TEST_ASSERT_EQUAL(ROF_ERR_IO, rof_temp_sensor_enumerate(&bus, list, 4, &count));
    TEST_ASSERT_EQUAL_size_t(0, count);
}

static void test_mock_sensor_allows_duplicate_ids(void)
{
    bus_setup();
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 30.0f);
    rof_sensor_info_t list[4];
    size_t count = 0;
    float temp_c = 0.0f;

    TEST_ASSERT_EQUAL(ROF_OK, rof_temp_sensor_enumerate(&bus, list, 4, &count));
    TEST_ASSERT_EQUAL_size_t(2, count);
    TEST_ASSERT_EQUAL_UINT64(list[0].id, list[1].id);
    /* read wirkt auf den zuerst angesteckten */
    TEST_ASSERT_EQUAL(ROF_OK, rof_temp_sensor_read(&bus, ID_A, &temp_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, temp_c);
}

static void test_mock_sensor_rejects_plug_when_full(void)
{
    bus_setup();
    for (rof_sensor_id_t id = 1; id <= MOCK_TEMP_SENSOR_MAX; id++) {
        TEST_ASSERT_EQUAL(ROF_OK, mock_temp_sensor_plug(&bus_mock, id, "DS18B20", 20.0f));
    }
    TEST_ASSERT_EQUAL(ROF_ERR_NO_SPACE, mock_temp_sensor_plug(&bus_mock, 999, "DS18B20", 20.0f));
}

static void test_mock_sensor_unknown_id_is_not_found(void)
{
    bus_setup();
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, mock_temp_sensor_unplug(&bus_mock, ID_A));
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, mock_temp_sensor_set_temp(&bus_mock, ID_A, 1.0f));
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND,
                      mock_temp_sensor_set_read_status(&bus_mock, ID_A, ROF_ERR_IO));
}

/* ---- Mock-Ausgang ------------------------------------------------------ */

static mock_output_t heater_mock;
static rof_output_t heater;

static void heater_setup(void)
{
    mock_output_init(&heater_mock);
    heater = mock_output_interface(&heater_mock);
}

static void test_mock_output_starts_off(void)
{
    heater_setup();
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, heater_mock.duty);
    TEST_ASSERT_EQUAL_UINT(0, heater_mock.set_calls);
}

static void test_mock_output_records_last_duty(void)
{
    heater_setup();
    TEST_ASSERT_EQUAL(ROF_OK, rof_output_set_duty(&heater, 0.25f));
    TEST_ASSERT_EQUAL(ROF_OK, rof_output_set_duty(&heater, 0.75f));

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.75f, heater_mock.duty);
    TEST_ASSERT_EQUAL_UINT(2, heater_mock.set_calls);
}

static void test_mock_output_accepts_bounds(void)
{
    heater_setup();
    TEST_ASSERT_EQUAL(ROF_OK, rof_output_set_duty(&heater, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, heater_mock.duty);
    TEST_ASSERT_EQUAL(ROF_OK, rof_output_set_duty(&heater, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, heater_mock.duty);
}

static void test_mock_output_rejects_invalid_duty_and_keeps_last(void)
{
    heater_setup();
    TEST_ASSERT_EQUAL(ROF_OK, rof_output_set_duty(&heater, 0.5f));

    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_output_set_duty(&heater, -0.1f));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_output_set_duty(&heater, 1.1f));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_output_set_duty(&heater, NAN));

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, heater_mock.duty);
    TEST_ASSERT_EQUAL_UINT(4, heater_mock.set_calls);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_fake_clock_starts_at_given_time);
    RUN_TEST(test_fake_clock_stands_still_until_advanced);
    RUN_TEST(test_fake_clock_handles_many_simulated_hours);

    RUN_TEST(test_mock_sensor_empty_bus_enumerates_nothing);
    RUN_TEST(test_mock_sensor_enumerates_plugged_sensors_in_order);
    RUN_TEST(test_mock_sensor_reads_current_temperature);
    RUN_TEST(test_mock_sensor_unplugged_sensor_disappears);
    RUN_TEST(test_mock_sensor_injected_read_error_keeps_value);
    RUN_TEST(test_mock_sensor_more_sensors_than_list_reports_no_space);
    RUN_TEST(test_mock_sensor_injected_bus_error);
    RUN_TEST(test_mock_sensor_allows_duplicate_ids);
    RUN_TEST(test_mock_sensor_rejects_plug_when_full);
    RUN_TEST(test_mock_sensor_unknown_id_is_not_found);

    RUN_TEST(test_mock_output_starts_off);
    RUN_TEST(test_mock_output_records_last_duty);
    RUN_TEST(test_mock_output_accepts_bounds);
    RUN_TEST(test_mock_output_rejects_invalid_duty_and_keeps_last);

    return UNITY_END();
}
