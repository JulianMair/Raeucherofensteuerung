/*
 * Tests der Mocks und der Fake-Uhr aus platform/host.
 * Später verlassen sich alle core-Tests auf diese Bausteine. Deshalb
 * werden sie selbst getestet. Alle Aufrufe laufen über die Interfaces
 * aus core/include, genau so, wie core sie benutzt (ARC-02).
 */
#include "unity.h"

#include "fake_clock.h"
#include "mock_log.h"
#include "mock_output.h"
#include "mock_storage.h"
#include "mock_temp_sensor.h"

#include <math.h>
#include <string.h>

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

/* ---- Mock-Speicher ----------------------------------------------------- */

static mock_storage_t flash_mock;
static rof_storage_t flash;

static void flash_setup(void)
{
    mock_storage_init(&flash_mock);
    flash = mock_storage_interface(&flash_mock);
}

static void test_mock_storage_unknown_key_is_not_found(void)
{
    flash_setup();
    uint8_t buf[8];
    size_t len = 99;

    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, rof_storage_load(&flash, "roles", buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL_size_t(99, len);
}

static void test_mock_storage_saves_and_loads_block(void)
{
    flash_setup();
    const uint8_t data[] = { 1, 2, 3, 4, 5 };
    uint8_t buf[16] = { 0 };
    size_t len = 0;

    TEST_ASSERT_EQUAL(ROF_OK, rof_storage_save(&flash, "roles", data, sizeof(data)));
    TEST_ASSERT_EQUAL(ROF_OK, rof_storage_load(&flash, "roles", buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL_size_t(sizeof(data), len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, buf, sizeof(data));
}

static void test_mock_storage_save_replaces_old_block(void)
{
    flash_setup();
    const uint8_t first[] = { 1, 2, 3, 4 };
    const uint8_t second[] = { 9, 8 };
    uint8_t buf[16] = { 0 };
    size_t len = 0;

    rof_storage_save(&flash, "roles", first, sizeof(first));
    TEST_ASSERT_EQUAL(ROF_OK, rof_storage_save(&flash, "roles", second, sizeof(second)));
    TEST_ASSERT_EQUAL(ROF_OK, rof_storage_load(&flash, "roles", buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL_size_t(sizeof(second), len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(second, buf, sizeof(second));
}

static void test_mock_storage_survives_restart(void)
{
    flash_setup();
    const uint8_t data[] = { 42 };
    rof_storage_save(&flash, "roles", data, sizeof(data));

    /* Neustart: core holt sich ein neues Interface, die Daten im Mock bleiben. */
    rof_storage_t after_restart = mock_storage_interface(&flash_mock);
    uint8_t buf[4] = { 0 };
    size_t len = 0;

    TEST_ASSERT_EQUAL(ROF_OK, rof_storage_load(&after_restart, "roles", buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL_UINT8(42, buf[0]);
}

static void test_mock_storage_too_small_buffer_is_no_space(void)
{
    flash_setup();
    const uint8_t data[] = { 1, 2, 3, 4 };
    uint8_t buf[2] = { 0 };
    size_t len = 99;
    rof_storage_save(&flash, "roles", data, sizeof(data));

    TEST_ASSERT_EQUAL(ROF_ERR_NO_SPACE, rof_storage_load(&flash, "roles", buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL_size_t(99, len);
    TEST_ASSERT_EQUAL_UINT8(0, buf[0]);
}

static void test_mock_storage_rejects_invalid_keys(void)
{
    flash_setup();
    const uint8_t data[] = { 1 };
    uint8_t buf[4];
    size_t len = 0;

    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_storage_save(&flash, "", data, sizeof(data)));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG,
                      rof_storage_save(&flash, "sixteen_chars_xx", data, sizeof(data)));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_storage_load(&flash, "", buf, sizeof(buf), &len));
    /* 15 Zeichen sind gerade noch erlaubt */
    TEST_ASSERT_EQUAL(ROF_OK, rof_storage_save(&flash, "fifteen_chars_x", data, sizeof(data)));
}

static void test_mock_storage_full_is_no_space(void)
{
    flash_setup();
    const uint8_t data[] = { 1 };
    char key[] = "key0";
    for (int i = 0; i < MOCK_STORAGE_SLOTS; i++) {
        key[3] = (char)('0' + i);
        TEST_ASSERT_EQUAL(ROF_OK, rof_storage_save(&flash, key, data, sizeof(data)));
    }
    TEST_ASSERT_EQUAL(ROF_ERR_NO_SPACE, rof_storage_save(&flash, "one_more", data, sizeof(data)));

    static uint8_t too_big[MOCK_STORAGE_VALUE_MAX + 1];
    TEST_ASSERT_EQUAL(ROF_ERR_NO_SPACE, rof_storage_save(&flash, "key0", too_big, sizeof(too_big)));
}

static void test_mock_storage_injected_errors(void)
{
    flash_setup();
    const uint8_t data[] = { 1 };
    uint8_t buf[4];
    size_t len = 0;

    mock_storage_set_save_status(&flash_mock, ROF_ERR_IO);
    TEST_ASSERT_EQUAL(ROF_ERR_IO, rof_storage_save(&flash, "roles", data, sizeof(data)));
    mock_storage_set_save_status(&flash_mock, ROF_OK);
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, rof_storage_load(&flash, "roles", buf, sizeof(buf), &len));

    rof_storage_save(&flash, "roles", data, sizeof(data));
    mock_storage_set_load_status(&flash_mock, ROF_ERR_IO);
    TEST_ASSERT_EQUAL(ROF_ERR_IO, rof_storage_load(&flash, "roles", buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL_UINT(2, flash_mock.save_calls);
}

/* ---- Mock-Log ---------------------------------------------------------- */

static mock_log_t logger_mock;
static rof_log_t logger_if;

static void logger_setup(void)
{
    mock_log_init(&logger_mock);
    logger_if = mock_log_interface(&logger_mock);
}

static void test_mock_log_starts_empty(void)
{
    logger_setup();
    for (int level = 0; level < ROF_LOG_LEVEL_COUNT; level++) {
        TEST_ASSERT_EQUAL_UINT(0, logger_mock.count[level]);
    }
    TEST_ASSERT_EQUAL_STRING("", logger_mock.last_msg);
}

static void test_mock_log_counts_per_level_and_keeps_last(void)
{
    logger_setup();
    rof_log_write(&logger_if, ROF_LOG_INFO, "start");
    rof_log_write(&logger_if, ROF_LOG_WARN, "sensor slow");
    rof_log_write(&logger_if, ROF_LOG_ERROR, "chamber sensor missing");

    TEST_ASSERT_EQUAL_UINT(1, logger_mock.count[ROF_LOG_ERROR]);
    TEST_ASSERT_EQUAL_UINT(1, logger_mock.count[ROF_LOG_WARN]);
    TEST_ASSERT_EQUAL_UINT(1, logger_mock.count[ROF_LOG_INFO]);
    TEST_ASSERT_EQUAL_UINT(0, logger_mock.count[ROF_LOG_DEBUG]);
    TEST_ASSERT_EQUAL(ROF_LOG_ERROR, logger_mock.last_level);
    TEST_ASSERT_EQUAL_STRING("chamber sensor missing", logger_mock.last_msg);
}

static void test_mock_log_copies_message(void)
{
    logger_setup();
    char msg[] = "original";
    rof_log_write(&logger_if, ROF_LOG_INFO, msg);
    msg[0] = 'X';

    TEST_ASSERT_EQUAL_STRING("Xriginal", msg);
    TEST_ASSERT_EQUAL_STRING("original", logger_mock.last_msg);
}

static void test_mock_log_truncates_long_message(void)
{
    logger_setup();
    char msg[MOCK_LOG_MSG_MAX + 10];
    for (size_t i = 0; i < sizeof(msg) - 1; i++) {
        msg[i] = 'a';
    }
    msg[sizeof(msg) - 1] = '\0';
    rof_log_write(&logger_if, ROF_LOG_DEBUG, msg);

    TEST_ASSERT_EQUAL_size_t(MOCK_LOG_MSG_MAX - 1, strlen(logger_mock.last_msg));
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

    RUN_TEST(test_mock_storage_unknown_key_is_not_found);
    RUN_TEST(test_mock_storage_saves_and_loads_block);
    RUN_TEST(test_mock_storage_save_replaces_old_block);
    RUN_TEST(test_mock_storage_survives_restart);
    RUN_TEST(test_mock_storage_too_small_buffer_is_no_space);
    RUN_TEST(test_mock_storage_rejects_invalid_keys);
    RUN_TEST(test_mock_storage_full_is_no_space);
    RUN_TEST(test_mock_storage_injected_errors);

    RUN_TEST(test_mock_log_starts_empty);
    RUN_TEST(test_mock_log_counts_per_level_and_keeps_last);
    RUN_TEST(test_mock_log_copies_message);
    RUN_TEST(test_mock_log_truncates_long_message);

    return UNITY_END();
}
