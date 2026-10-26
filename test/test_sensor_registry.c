/*
 * Tests der Sensorverwaltung core/sensors (SEN-01 bis SEN-07).
 * Jeder Test nennt im Kommentar die Anforderung, die er belegt.
 */
#include "unity.h"

#include "mock_log.h"
#include "mock_storage.h"
#include "mock_temp_sensor.h"
#include "rof_sensor_registry.h"

#include <math.h>

#define ID_A 0x28AA000000000001ULL
#define ID_B 0x28BB000000000002ULL
#define ID_C 0x28CC000000000003ULL

static mock_temp_sensor_t bus_mock;
static rof_temp_sensor_t bus;
static const rof_temp_sensor_t *drivers[1];
static mock_storage_t flash_mock;
static rof_storage_t flash;
static mock_log_t log_mock;
static rof_log_t logger_if;
static rof_sensor_registry_t reg;

void setUp(void)
{
    mock_temp_sensor_init(&bus_mock);
    bus = mock_temp_sensor_interface(&bus_mock);
    drivers[0] = &bus;
    mock_storage_init(&flash_mock);
    flash = mock_storage_interface(&flash_mock);
    mock_log_init(&log_mock);
    logger_if = mock_log_interface(&log_mock);
    TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_init(&reg, drivers, 1, &flash, &logger_if));
}

void tearDown(void) {}

/* Kanal mit dieser ID oder NULL */
static const rof_sensor_channel_t *channel_of(rof_sensor_id_t id)
{
    for (size_t i = 0; i < rof_sensor_registry_channel_count(&reg); i++) {
        const rof_sensor_channel_t *ch = rof_sensor_registry_channel(&reg, i);
        if (ch->id == id) {
            return ch;
        }
    }
    return NULL;
}

/* ---- Initialisierung ---------------------------------------------------- */

/* E-007: Interfaces werden bei der Initialisierung geprüft */
static void test_init_rejects_missing_interfaces(void)
{
    rof_sensor_registry_t other;
    rof_temp_sensor_t broken = bus;
    broken.read = NULL;
    const rof_temp_sensor_t *broken_drivers[1] = { &broken };
    const rof_temp_sensor_t *five[5] = { &bus, &bus, &bus, &bus, &bus };
    rof_storage_t no_save = flash;
    no_save.save = NULL;

    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_sensor_registry_init(&other, drivers, 0, &flash, &logger_if));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_sensor_registry_init(&other, five, 5, &flash, &logger_if));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_sensor_registry_init(&other, NULL, 1, &flash, &logger_if));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_sensor_registry_init(&other, broken_drivers, 1, &flash, &logger_if));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_sensor_registry_init(&other, drivers, 1, NULL, &logger_if));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_sensor_registry_init(&other, drivers, 1, &no_save, &logger_if));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_sensor_registry_init(&other, drivers, 1, &flash, NULL));
}

/* ---- Erkennen von Sensoren ---------------------------------------------- */

/* SEN-03 */
static void test_empty_bus_has_no_channels(void)
{
    rof_sensor_registry_update(&reg);

    TEST_ASSERT_EQUAL_size_t(0, rof_sensor_registry_channel_count(&reg));
}

/* SEN-01, SEN-03 */
static void test_plugged_sensors_appear_as_unassigned_channels(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    mock_temp_sensor_plug(&bus_mock, ID_B, "DS18B20", 21.0f);

    rof_sensor_registry_update(&reg);

    TEST_ASSERT_EQUAL_size_t(2, rof_sensor_registry_channel_count(&reg));
    const rof_sensor_channel_t *a = rof_sensor_registry_channel(&reg, 0);
    TEST_ASSERT_EQUAL_UINT64(ID_A, a->id);
    TEST_ASSERT_EQUAL_STRING("DS18B20", a->type);
    TEST_ASSERT_EQUAL(ROF_ROLE_UNASSIGNED, a->role);
    TEST_ASSERT_TRUE(a->present);
    TEST_ASSERT_EQUAL_UINT64(ID_B, rof_sensor_registry_channel(&reg, 1)->id);
}

/* SEN-03 */
static void test_new_sensor_is_logged_once(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);

    rof_sensor_registry_update(&reg);
    rof_sensor_registry_update(&reg);

    TEST_ASSERT_EQUAL_UINT(1, log_mock.count[ROF_LOG_INFO]);
}

/* SEN-03 */
static void test_unplugged_unassigned_sensor_frees_its_channel(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    mock_temp_sensor_plug(&bus_mock, ID_B, "DS18B20", 21.0f);
    rof_sensor_registry_update(&reg);

    mock_temp_sensor_unplug(&bus_mock, ID_A);
    rof_sensor_registry_update(&reg);

    TEST_ASSERT_EQUAL_size_t(1, rof_sensor_registry_channel_count(&reg));
    TEST_ASSERT_NULL(channel_of(ID_A));
    TEST_ASSERT_NOT_NULL(channel_of(ID_B));
}

/* SEN-03 */
static void test_replugged_sensor_gets_a_channel_again(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    rof_sensor_registry_update(&reg);
    mock_temp_sensor_unplug(&bus_mock, ID_A);
    rof_sensor_registry_update(&reg);

    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    rof_sensor_registry_update(&reg);

    TEST_ASSERT_NOT_NULL(channel_of(ID_A));
    TEST_ASSERT_TRUE(channel_of(ID_A)->present);
}

static void test_bus_error_is_logged_and_sensors_count_as_absent(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    rof_sensor_registry_update(&reg);

    mock_temp_sensor_set_enumerate_status(&bus_mock, ROF_ERR_IO);
    rof_sensor_registry_update(&reg);

    TEST_ASSERT_NULL(channel_of(ID_A));
    TEST_ASSERT_EQUAL_UINT(1, log_mock.count[ROF_LOG_ERROR]);
}

static void test_channel_index_out_of_range_is_null(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    rof_sensor_registry_update(&reg);

    TEST_ASSERT_NOT_NULL(rof_sensor_registry_channel(&reg, 0));
    TEST_ASSERT_NULL(rof_sensor_registry_channel(&reg, 1));
}

/* ---- Messwerte und Kalibrieroffset -------------------------------------- */

/* SEN-01 */
static void test_update_reads_temperatures(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.5f);

    rof_sensor_registry_update(&reg);

    const rof_sensor_channel_t *a = channel_of(ID_A);
    TEST_ASSERT_EQUAL(ROF_OK, a->read_status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.5f, a->raw_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.5f, a->temp_c);
}

/* SEN-01 */
static void test_offset_is_added_to_reading(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    rof_sensor_registry_update(&reg);

    TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_set_offset(&reg, ID_A, -1.5f));
    rof_sensor_registry_update(&reg);

    const rof_sensor_channel_t *a = channel_of(ID_A);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.5f, a->offset_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, a->raw_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 18.5f, a->temp_c);
}

/* SEN-01: ein neuer Offset wirkt sofort, nicht erst beim nächsten update() */
static void test_offset_change_applies_immediately(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    rof_sensor_registry_update(&reg);

    rof_sensor_registry_set_offset(&reg, ID_A, 2.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.0f, channel_of(ID_A)->temp_c);
}

/* SEN-01, E-014: ±5 °C erlaubt, alles andere abgelehnt statt begrenzt */
static void test_offset_outside_limits_is_rejected(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    rof_sensor_registry_update(&reg);

    TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_set_offset(&reg, ID_A, 5.0f));
    TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_set_offset(&reg, ID_A, -5.0f));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_sensor_registry_set_offset(&reg, ID_A, 5.01f));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_sensor_registry_set_offset(&reg, ID_A, -5.01f));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_sensor_registry_set_offset(&reg, ID_A, NAN));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.0f, channel_of(ID_A)->offset_c);
}

static void test_offset_for_unknown_sensor_is_not_found(void)
{
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, rof_sensor_registry_set_offset(&reg, ID_A, 1.0f));
}

/* Ein Lesefehler überschreibt den letzten gültigen Wert nicht */
static void test_read_error_keeps_last_value(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    rof_sensor_registry_update(&reg);

    mock_temp_sensor_set_read_status(&bus_mock, ID_A, ROF_ERR_IO);
    mock_temp_sensor_set_temp(&bus_mock, ID_A, 99.0f);
    rof_sensor_registry_update(&reg);

    const rof_sensor_channel_t *a = channel_of(ID_A);
    TEST_ASSERT_EQUAL(ROF_ERR_IO, a->read_status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, a->temp_c);
}

/* Ein Sensor mit Offset behält seinen Kanal, auch wenn er abgezogen wird */
static void test_unplugged_sensor_with_offset_keeps_channel(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    rof_sensor_registry_update(&reg);
    rof_sensor_registry_set_offset(&reg, ID_A, 1.0f);

    mock_temp_sensor_unplug(&bus_mock, ID_A);
    rof_sensor_registry_update(&reg);

    const rof_sensor_channel_t *a = channel_of(ID_A);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_FALSE(a->present);
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, a->read_status);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_rejects_missing_interfaces);

    RUN_TEST(test_empty_bus_has_no_channels);
    RUN_TEST(test_plugged_sensors_appear_as_unassigned_channels);
    RUN_TEST(test_new_sensor_is_logged_once);
    RUN_TEST(test_unplugged_unassigned_sensor_frees_its_channel);
    RUN_TEST(test_replugged_sensor_gets_a_channel_again);
    RUN_TEST(test_bus_error_is_logged_and_sensors_count_as_absent);
    RUN_TEST(test_channel_index_out_of_range_is_null);

    RUN_TEST(test_update_reads_temperatures);
    RUN_TEST(test_offset_is_added_to_reading);
    RUN_TEST(test_offset_change_applies_immediately);
    RUN_TEST(test_offset_outside_limits_is_rejected);
    RUN_TEST(test_offset_for_unknown_sensor_is_not_found);
    RUN_TEST(test_read_error_keeps_last_value);
    RUN_TEST(test_unplugged_sensor_with_offset_keeps_channel);

    return UNITY_END();
}
