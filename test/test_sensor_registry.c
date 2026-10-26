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
#include <string.h>

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

/* Kanal mit dieser ID. Fehlt er, scheitert der Test hier, statt später
 * beim Zugriff über einen NULL-Zeiger das ganze Testprogramm abzubrechen. */
static const rof_sensor_channel_t *existing_channel(rof_sensor_id_t id)
{
    const rof_sensor_channel_t *ch = channel_of(id);
    TEST_ASSERT_NOT_NULL_MESSAGE(ch, "Kanal fehlt");
    return ch;
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
    TEST_ASSERT_TRUE(existing_channel(ID_A)->present);
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

    const rof_sensor_channel_t *a = existing_channel(ID_A);
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

    const rof_sensor_channel_t *a = existing_channel(ID_A);
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

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.0f, existing_channel(ID_A)->temp_c);
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

    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.0f, existing_channel(ID_A)->offset_c);
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

    const rof_sensor_channel_t *a = existing_channel(ID_A);
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

    const rof_sensor_channel_t *a = existing_channel(ID_A);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_FALSE(a->present);
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, a->read_status);
}

/* ---- Rollen zuweisen und speichern -------------------------------------- */

static void plug_and_update(rof_sensor_id_t id, float temp_c)
{
    mock_temp_sensor_plug(&bus_mock, id, "DS18B20", temp_c);
    rof_sensor_registry_update(&reg);
}

/* SEN-02, SEN-04 */
static void test_assign_sets_role_and_saves(void)
{
    plug_and_update(ID_A, 20.0f);

    TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER));

    TEST_ASSERT_EQUAL(ROF_ROLE_CHAMBER, existing_channel(ID_A)->role);
    TEST_ASSERT_EQUAL_UINT(1, flash_mock.save_calls);
}

/* SEN-02 */
static void test_all_roles_can_be_assigned_to_different_sensors(void)
{
    const rof_role_t roles[] = { ROF_ROLE_CHAMBER, ROF_ROLE_PROBE_1, ROF_ROLE_PROBE_2,
                                 ROF_ROLE_SMOKE_GEN };
    for (rof_sensor_id_t id = 1; id <= 4; id++) {
        plug_and_update(id, 20.0f);
        TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_assign(&reg, id, roles[id - 1]));
    }
    for (rof_sensor_id_t id = 1; id <= 4; id++) {
        TEST_ASSERT_EQUAL(roles[id - 1], existing_channel(id)->role);
    }
}

/* SEN-02, E-014: eine vergebene Rolle wird verschoben */
static void test_assign_moves_role_from_previous_sensor(void)
{
    plug_and_update(ID_A, 20.0f);
    plug_and_update(ID_B, 21.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);

    TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_assign(&reg, ID_B, ROF_ROLE_CHAMBER));

    TEST_ASSERT_EQUAL(ROF_ROLE_UNASSIGNED, existing_channel(ID_A)->role);
    TEST_ASSERT_EQUAL(ROF_ROLE_CHAMBER, existing_channel(ID_B)->role);
}

/* SEN-02 */
static void test_assign_unassigned_releases_role(void)
{
    plug_and_update(ID_A, 20.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_PROBE_1);

    TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_UNASSIGNED));

    TEST_ASSERT_EQUAL(ROF_ROLE_UNASSIGNED, existing_channel(ID_A)->role);
}

static void test_assign_rejects_unknown_sensor(void)
{
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER));
    TEST_ASSERT_EQUAL_UINT(0, flash_mock.save_calls);
}

static void test_assign_rejects_invalid_role(void)
{
    plug_and_update(ID_A, 20.0f);

    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_COUNT));
    TEST_ASSERT_EQUAL(ROF_ERR_INVALID_ARG, rof_sensor_registry_assign(&reg, ID_A, (rof_role_t)99));
    TEST_ASSERT_EQUAL(ROF_ROLE_UNASSIGNED, existing_channel(ID_A)->role);
}

/* SEN-04: Was nicht gespeichert werden kann, gilt auch im RAM nicht */
static void test_assign_is_undone_when_saving_fails(void)
{
    plug_and_update(ID_A, 20.0f);
    plug_and_update(ID_B, 21.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);
    mock_storage_set_save_status(&flash_mock, ROF_ERR_IO);

    TEST_ASSERT_EQUAL(ROF_ERR_IO, rof_sensor_registry_assign(&reg, ID_B, ROF_ROLE_CHAMBER));

    TEST_ASSERT_EQUAL(ROF_ROLE_CHAMBER, existing_channel(ID_A)->role);
    TEST_ASSERT_EQUAL(ROF_ROLE_UNASSIGNED, existing_channel(ID_B)->role);
}

/* SEN-04 */
static void test_offset_is_saved_and_undone_when_saving_fails(void)
{
    plug_and_update(ID_A, 20.0f);
    TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_set_offset(&reg, ID_A, 1.0f));
    TEST_ASSERT_EQUAL_UINT(1, flash_mock.save_calls);

    mock_storage_set_save_status(&flash_mock, ROF_ERR_IO);
    TEST_ASSERT_EQUAL(ROF_ERR_IO, rof_sensor_registry_set_offset(&reg, ID_A, 3.0f));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, existing_channel(ID_A)->offset_c);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 21.0f, existing_channel(ID_A)->temp_c);
}

/* SEN-04: Format Version 1 = Version, Anzahl, je Eintrag ID (8), Rolle (1), Offset (4) */
static void test_saved_block_has_version_count_and_entries(void)
{
    plug_and_update(ID_A, 20.0f);
    plug_and_update(ID_B, 21.0f);
    rof_sensor_registry_assign(&reg, ID_B, ROF_ROLE_PROBE_2);
    uint8_t buf[128];
    size_t len = 0;

    TEST_ASSERT_EQUAL(ROF_OK, rof_storage_load(&flash, ROF_SENSOR_STORAGE_KEY, buf, sizeof(buf), &len));

    /* ID_A hat weder Rolle noch Offset und wird nicht gespeichert */
    TEST_ASSERT_EQUAL_size_t(2 + 13, len);
    TEST_ASSERT_EQUAL_UINT8(1, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(1, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0x02, buf[2]); /* niederwertigstes Byte von ID_B zuerst */
    TEST_ASSERT_EQUAL_UINT8(0x28, buf[9]);
    TEST_ASSERT_EQUAL_UINT8(ROF_ROLE_PROBE_2, buf[10]);
}

/* SEN-05 Vorbereitung: ein zugeordneter Sensor behält seinen Kanal, wenn er fehlt */
static void test_unplugged_assigned_sensor_keeps_channel(void)
{
    plug_and_update(ID_A, 20.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);

    mock_temp_sensor_unplug(&bus_mock, ID_A);
    rof_sensor_registry_update(&reg);

    TEST_ASSERT_NOT_NULL(channel_of(ID_A));
    TEST_ASSERT_FALSE(existing_channel(ID_A)->present);
}

/* ---- Wiederherstellen nach Neustart ------------------------------------- */

/* Neustart: core wird neu initialisiert, Bus und Speicher bleiben. */
static void restart(void)
{
    TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_init(&reg, drivers, 1, &flash, &logger_if));
}

/* Ein Eintrag im Speicherformat Version 1, siehe test_saved_block_has_version_count_and_entries */
static void put_entry(uint8_t *dst, uint64_t id, uint8_t role, float offset_c)
{
    uint32_t bits;
    memcpy(&bits, &offset_c, sizeof(bits));
    for (unsigned i = 0; i < 8; i++) {
        dst[i] = (uint8_t)(id >> (8u * i));
    }
    dst[8] = role;
    for (unsigned i = 0; i < 4; i++) {
        dst[9 + i] = (uint8_t)(bits >> (8u * i));
    }
}

/* SEN-04 */
static void test_restart_restores_roles_and_offsets(void)
{
    plug_and_update(ID_A, 20.0f);
    plug_and_update(ID_B, 30.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);
    rof_sensor_registry_assign(&reg, ID_B, ROF_ROLE_PROBE_1);
    rof_sensor_registry_set_offset(&reg, ID_B, 1.5f);

    restart();

    /* Vor dem ersten update() sind die Kanäle bekannt, aber noch nicht gefunden. */
    TEST_ASSERT_EQUAL_size_t(2, rof_sensor_registry_channel_count(&reg));
    TEST_ASSERT_EQUAL(ROF_ROLE_CHAMBER, existing_channel(ID_A)->role);
    TEST_ASSERT_FALSE(existing_channel(ID_A)->present);
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, existing_channel(ID_A)->read_status);
    TEST_ASSERT_EQUAL_STRING("", existing_channel(ID_A)->type);

    rof_sensor_registry_update(&reg);

    TEST_ASSERT_TRUE(existing_channel(ID_A)->present);
    TEST_ASSERT_EQUAL_STRING("DS18B20", existing_channel(ID_A)->type);
    TEST_ASSERT_EQUAL(ROF_ROLE_PROBE_1, existing_channel(ID_B)->role);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 31.5f, existing_channel(ID_B)->temp_c);
}

/* SEN-04: erster Start ohne gespeicherte Zuordnung ist kein Fehler */
static void test_first_start_has_no_channels_and_no_error(void)
{
    TEST_ASSERT_EQUAL_size_t(0, rof_sensor_registry_channel_count(&reg));
    TEST_ASSERT_EQUAL_UINT(0, log_mock.count[ROF_LOG_ERROR]);
}

/* SEN-04: Speicher nicht lesbar, Start ohne Zuordnung */
static void test_unreadable_storage_starts_without_assignments(void)
{
    plug_and_update(ID_A, 20.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);
    mock_storage_set_load_status(&flash_mock, ROF_ERR_IO);

    restart();

    TEST_ASSERT_EQUAL_size_t(0, rof_sensor_registry_channel_count(&reg));
    TEST_ASSERT_EQUAL_UINT(1, log_mock.count[ROF_LOG_ERROR]);
}

/* Gegenprobe zu den kaputten Blöcken: put_entry erzeugt gültige Daten */
static void test_handcrafted_valid_block_is_restored(void)
{
    uint8_t blob[2 + 2 * 13] = { 1, 2 };
    put_entry(&blob[2], ID_A, ROF_ROLE_CHAMBER, 0.0f);
    put_entry(&blob[15], ID_B, ROF_ROLE_UNASSIGNED, -2.0f);
    rof_storage_save(&flash, ROF_SENSOR_STORAGE_KEY, blob, sizeof(blob));

    restart();

    TEST_ASSERT_EQUAL(ROF_ROLE_CHAMBER, existing_channel(ID_A)->role);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.0f, existing_channel(ID_B)->offset_c);
}

/* SEN-04: kaputte oder fremde Daten werden komplett verworfen und gemeldet */
static void check_block_is_discarded(const uint8_t *blob, size_t len)
{
    mock_storage_init(&flash_mock);
    mock_log_init(&log_mock);
    rof_storage_save(&flash, ROF_SENSOR_STORAGE_KEY, blob, len);

    restart();

    TEST_ASSERT_EQUAL_size_t(0, rof_sensor_registry_channel_count(&reg));
    TEST_ASSERT_EQUAL_UINT(1, log_mock.count[ROF_LOG_ERROR]);
}

static void test_corrupt_stored_blocks_are_discarded(void)
{
    uint8_t blob[2 + 9 * 13] = { 0 };

    const uint8_t too_short[] = { 1 };
    check_block_is_discarded(too_short, sizeof(too_short));

    const uint8_t unknown_version[] = { 2, 0 };
    check_block_is_discarded(unknown_version, sizeof(unknown_version));

    blob[0] = 1;
    blob[1] = 9; /* mehr Einträge als Kanäle */
    for (unsigned i = 0; i < 9; i++) {
        put_entry(&blob[2 + i * 13], 100 + i, ROF_ROLE_UNASSIGNED, 1.0f);
    }
    check_block_is_discarded(blob, sizeof(blob));

    blob[1] = 1; /* Länge passt nicht zur Anzahl */
    check_block_is_discarded(blob, 2 + 13 + 1);

    put_entry(&blob[2], ID_A, 7, 0.0f); /* keine gültige Rolle */
    check_block_is_discarded(blob, 2 + 13);

    put_entry(&blob[2], ID_A, ROF_ROLE_CHAMBER, 6.0f); /* Offset außerhalb */
    check_block_is_discarded(blob, 2 + 13);

    put_entry(&blob[2], ID_A, ROF_ROLE_CHAMBER, NAN);
    check_block_is_discarded(blob, 2 + 13);

    blob[1] = 2;
    put_entry(&blob[2], ID_A, ROF_ROLE_CHAMBER, 0.0f);
    put_entry(&blob[15], ID_A, ROF_ROLE_PROBE_1, 0.0f); /* ID doppelt */
    check_block_is_discarded(blob, 2 + 2 * 13);

    put_entry(&blob[15], ID_B, ROF_ROLE_CHAMBER, 0.0f); /* Rolle doppelt */
    check_block_is_discarded(blob, 2 + 2 * 13);
}

/* ---- Rollenstatus ------------------------------------------------------- */

/* SEN-05 */
static void test_role_without_sensor_is_unassigned(void)
{
    float temp_c = -1.0f;

    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_UNASSIGNED, rof_sensor_registry_role(&reg, ROF_ROLE_CHAMBER, &temp_c));
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_UNASSIGNED, rof_sensor_registry_role(&reg, ROF_ROLE_UNASSIGNED, &temp_c));
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_UNASSIGNED, rof_sensor_registry_role(&reg, (rof_role_t)99, &temp_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, temp_c);
}

/* SEN-05 */
static void test_role_reports_temperature_with_offset(void)
{
    plug_and_update(ID_A, 80.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);
    rof_sensor_registry_set_offset(&reg, ID_A, -0.5f);
    float temp_c = 0.0f;

    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, rof_sensor_registry_role(&reg, ROF_ROLE_CHAMBER, &temp_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 79.5f, temp_c);
}

/* SEN-05: fehlt ein zugeordneter Sensor, meldet seine Rolle einen Fehler */
static void test_missing_assigned_sensor_reports_missing(void)
{
    plug_and_update(ID_A, 80.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);
    mock_temp_sensor_unplug(&bus_mock, ID_A);
    rof_sensor_registry_update(&reg);
    float temp_c = -1.0f;

    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_MISSING, rof_sensor_registry_role(&reg, ROF_ROLE_CHAMBER, &temp_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, temp_c);
}

/* SEN-05: gemeldet wird einmal je Ausfall, nicht bei jedem update() */
static void test_missing_sensor_is_logged_once_per_loss(void)
{
    plug_and_update(ID_A, 80.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);

    mock_temp_sensor_unplug(&bus_mock, ID_A);
    rof_sensor_registry_update(&reg);
    rof_sensor_registry_update(&reg);
    TEST_ASSERT_EQUAL_UINT(1, log_mock.count[ROF_LOG_ERROR]);

    plug_and_update(ID_A, 80.0f);
    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, rof_sensor_registry_role(&reg, ROF_ROLE_CHAMBER, &temp_c));

    mock_temp_sensor_unplug(&bus_mock, ID_A);
    rof_sensor_registry_update(&reg);
    TEST_ASSERT_EQUAL_UINT(2, log_mock.count[ROF_LOG_ERROR]);
}

/* SEN-05 */
static void test_read_error_reports_read_error(void)
{
    plug_and_update(ID_A, 80.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_PROBE_1);
    mock_temp_sensor_set_read_status(&bus_mock, ID_A, ROF_ERR_IO);
    rof_sensor_registry_update(&reg);
    float temp_c = -1.0f;

    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_READ_ERROR, rof_sensor_registry_role(&reg, ROF_ROLE_PROBE_1, &temp_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, temp_c);
}

/* SEN-04, SEN-05: nach einem Neustart fehlt die Rolle, bis der Sensor gefunden ist */
static void test_restored_role_is_missing_until_sensor_found(void)
{
    plug_and_update(ID_A, 80.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);
    restart();
    float temp_c = 0.0f;

    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_MISSING, rof_sensor_registry_role(&reg, ROF_ROLE_CHAMBER, &temp_c));

    rof_sensor_registry_update(&reg);
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, rof_sensor_registry_role(&reg, ROF_ROLE_CHAMBER, &temp_c));
}

/* SEN-05: fehlt der Sensor schon beim Start, wird das ebenfalls gemeldet */
static void test_sensor_missing_at_start_is_logged(void)
{
    plug_and_update(ID_A, 80.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);
    mock_temp_sensor_unplug(&bus_mock, ID_A);
    restart();

    rof_sensor_registry_update(&reg);

    TEST_ASSERT_EQUAL_UINT(1, log_mock.count[ROF_LOG_ERROR]);
}

/* SEN-05: ein Busfehler lässt die zugeordneten Sensoren des Busses fehlen */
static void test_bus_error_makes_assigned_role_missing(void)
{
    plug_and_update(ID_A, 80.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);
    mock_temp_sensor_set_enumerate_status(&bus_mock, ROF_ERR_IO);
    rof_sensor_registry_update(&reg);
    float temp_c = 0.0f;

    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_MISSING, rof_sensor_registry_role(&reg, ROF_ROLE_CHAMBER, &temp_c));
}

/* ---- Doppelte IDs und zu viele Sensoren --------------------------------- */

/* Speichert ID_A als Garraum und startet neu, ohne den Bus zu ändern. */
static void restart_with_chamber_a(void)
{
    uint8_t blob[2 + 13] = { 1, 1 };
    put_entry(&blob[2], ID_A, ROF_ROLE_CHAMBER, 0.0f);
    rof_storage_save(&flash, ROF_SENSOR_STORAGE_KEY, blob, sizeof(blob));
    restart();
}

/* SEN-07 */
static void test_duplicate_id_gives_one_channel_marked_duplicate(void)
{
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 20.0f);
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 30.0f);

    rof_sensor_registry_update(&reg);

    TEST_ASSERT_EQUAL_size_t(1, rof_sensor_registry_channel_count(&reg));
    TEST_ASSERT_TRUE(existing_channel(ID_A)->duplicate);
    TEST_ASSERT_EQUAL_UINT(1, log_mock.count[ROF_LOG_ERROR]);
}

/* SEN-07: bei doppelter ID ist unklar, welcher Sensor antwortet. Kein Wert. */
static void test_duplicate_assigned_sensor_reports_duplicate(void)
{
    plug_and_update(ID_A, 20.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 30.0f);
    const unsigned reads_before = bus_mock.read_calls;

    rof_sensor_registry_update(&reg);

    float temp_c = -1.0f;
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_DUPLICATE, rof_sensor_registry_role(&reg, ROF_ROLE_CHAMBER, &temp_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, temp_c);
    TEST_ASSERT_EQUAL_UINT(reads_before, bus_mock.read_calls);
}

/* SEN-07 */
static void test_duplicate_is_logged_once_and_clears_when_resolved(void)
{
    plug_and_update(ID_A, 20.0f);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 30.0f);
    rof_sensor_registry_update(&reg);
    rof_sensor_registry_update(&reg);
    TEST_ASSERT_EQUAL_UINT(1, log_mock.count[ROF_LOG_ERROR]);

    mock_temp_sensor_unplug(&bus_mock, ID_A); /* einer der beiden bleibt */
    rof_sensor_registry_update(&reg);

    float temp_c = 0.0f;
    TEST_ASSERT_FALSE(existing_channel(ID_A)->duplicate);
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, rof_sensor_registry_role(&reg, ROF_ROLE_CHAMBER, &temp_c));
}

/* SEN-07 */
static void test_nine_sensors_fill_eight_channels_and_report_overflow(void)
{
    for (rof_sensor_id_t id = 1; id <= 9; id++) {
        mock_temp_sensor_plug(&bus_mock, id, "DS18B20", 20.0f);
    }

    rof_sensor_registry_update(&reg);
    rof_sensor_registry_update(&reg);

    TEST_ASSERT_EQUAL_size_t(ROF_SENSOR_CHANNELS_MAX, rof_sensor_registry_channel_count(&reg));
    TEST_ASSERT_NOT_NULL(channel_of(8));
    TEST_ASSERT_NULL(channel_of(9));
    TEST_ASSERT_TRUE(rof_sensor_registry_overflow(&reg));
    TEST_ASSERT_EQUAL_UINT(1, log_mock.count[ROF_LOG_WARN]);
}

/* SEN-07: ein zugeordneter Sensor behält seinen Kanal, auch wenn er auf
 * dem Bus hinter acht neuen Sensoren kommt */
static void test_assigned_sensor_has_priority_over_new_sensors(void)
{
    for (rof_sensor_id_t id = 1; id <= 8; id++) {
        mock_temp_sensor_plug(&bus_mock, id, "DS18B20", 20.0f);
    }
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 80.0f);
    restart_with_chamber_a();

    rof_sensor_registry_update(&reg);

    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, rof_sensor_registry_role(&reg, ROF_ROLE_CHAMBER, &temp_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 80.0f, temp_c);
    TEST_ASSERT_EQUAL_size_t(ROF_SENSOR_CHANNELS_MAX, rof_sensor_registry_channel_count(&reg));
    TEST_ASSERT_NULL(channel_of(8));
    TEST_ASSERT_TRUE(rof_sensor_registry_overflow(&reg));
}

/* SEN-07: hinter mehr als ROF_SENSOR_SCAN_MAX Sensoren wird ein zugeordneter
 * Sensor nicht mehr gefunden. Er gilt dann als fehlend; das ist sicher. */
static void test_assigned_sensor_beyond_scan_limit_counts_as_missing(void)
{
    for (rof_sensor_id_t id = 1; id <= ROF_SENSOR_SCAN_MAX; id++) {
        mock_temp_sensor_plug(&bus_mock, id, "DS18B20", 20.0f);
    }
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 80.0f);
    restart_with_chamber_a();

    rof_sensor_registry_update(&reg);

    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_MISSING, rof_sensor_registry_role(&reg, ROF_ROLE_CHAMBER, &temp_c));
    TEST_ASSERT_TRUE(rof_sensor_registry_overflow(&reg));
}

/* SEN-07 */
static void test_overflow_clears_when_sensors_are_removed(void)
{
    for (rof_sensor_id_t id = 1; id <= 9; id++) {
        mock_temp_sensor_plug(&bus_mock, id, "DS18B20", 20.0f);
    }
    rof_sensor_registry_update(&reg);

    mock_temp_sensor_unplug(&bus_mock, 9);
    rof_sensor_registry_update(&reg);

    TEST_ASSERT_FALSE(rof_sensor_registry_overflow(&reg));
}

/* ---- Zweiter Treibertyp ------------------------------------------------- */

#define ID_PT 0x3100000000000001ULL

static mock_temp_sensor_t pt_mock;
static rof_temp_sensor_t pt_bus;

/* Zweiter Treiber, der wie ein PT1000 über MAX31865 seinen Typ meldet */
static void init_with_two_drivers(void)
{
    mock_temp_sensor_init(&pt_mock);
    pt_bus = mock_temp_sensor_interface(&pt_mock);
    static const rof_temp_sensor_t *two[2];
    two[0] = &bus;
    two[1] = &pt_bus;
    TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_init(&reg, two, 2, &flash, &logger_if));
}

/* SEN-06: ein neuer Sensortyp braucht nur einen Treiber, keine Änderung in core */
static void test_second_driver_type_works_without_core_changes(void)
{
    init_with_two_drivers();
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 80.0f);
    mock_temp_sensor_plug(&pt_mock, ID_PT, "PT1000", 65.0f);
    rof_sensor_registry_update(&reg);

    TEST_ASSERT_EQUAL_STRING("DS18B20", existing_channel(ID_A)->type);
    TEST_ASSERT_EQUAL_STRING("PT1000", existing_channel(ID_PT)->type);

    TEST_ASSERT_EQUAL(ROF_OK, rof_sensor_registry_assign(&reg, ID_PT, ROF_ROLE_PROBE_1));
    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, rof_sensor_registry_role(&reg, ROF_ROLE_PROBE_1, &temp_c));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 65.0f, temp_c);
    TEST_ASSERT_EQUAL_UINT(1, pt_mock.read_calls);
    TEST_ASSERT_EQUAL_UINT(1, bus_mock.read_calls);
}

/* SEN-06, SEN-07: dieselbe ID von zwei Treibern ist ebenfalls doppelt */
static void test_same_id_on_two_drivers_is_duplicate(void)
{
    init_with_two_drivers();
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 80.0f);
    mock_temp_sensor_plug(&pt_mock, ID_A, "PT1000", 65.0f);

    rof_sensor_registry_update(&reg);

    TEST_ASSERT_TRUE(existing_channel(ID_A)->duplicate);
}

/* SEN-06: ein Busfehler trifft nur die Sensoren dieses Treibers */
static void test_bus_error_affects_only_its_own_driver(void)
{
    init_with_two_drivers();
    mock_temp_sensor_plug(&bus_mock, ID_A, "DS18B20", 80.0f);
    mock_temp_sensor_plug(&pt_mock, ID_PT, "PT1000", 65.0f);
    rof_sensor_registry_update(&reg);
    rof_sensor_registry_assign(&reg, ID_A, ROF_ROLE_CHAMBER);
    rof_sensor_registry_assign(&reg, ID_PT, ROF_ROLE_PROBE_1);

    mock_temp_sensor_set_enumerate_status(&bus_mock, ROF_ERR_IO);
    rof_sensor_registry_update(&reg);

    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_MISSING, rof_sensor_registry_role(&reg, ROF_ROLE_CHAMBER, &temp_c));
    TEST_ASSERT_EQUAL(ROF_ROLE_STATUS_OK, rof_sensor_registry_role(&reg, ROF_ROLE_PROBE_1, &temp_c));
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

    RUN_TEST(test_assign_sets_role_and_saves);
    RUN_TEST(test_all_roles_can_be_assigned_to_different_sensors);
    RUN_TEST(test_assign_moves_role_from_previous_sensor);
    RUN_TEST(test_assign_unassigned_releases_role);
    RUN_TEST(test_assign_rejects_unknown_sensor);
    RUN_TEST(test_assign_rejects_invalid_role);
    RUN_TEST(test_assign_is_undone_when_saving_fails);
    RUN_TEST(test_offset_is_saved_and_undone_when_saving_fails);
    RUN_TEST(test_saved_block_has_version_count_and_entries);
    RUN_TEST(test_unplugged_assigned_sensor_keeps_channel);

    RUN_TEST(test_restart_restores_roles_and_offsets);
    RUN_TEST(test_first_start_has_no_channels_and_no_error);
    RUN_TEST(test_unreadable_storage_starts_without_assignments);
    RUN_TEST(test_handcrafted_valid_block_is_restored);
    RUN_TEST(test_corrupt_stored_blocks_are_discarded);

    RUN_TEST(test_role_without_sensor_is_unassigned);
    RUN_TEST(test_role_reports_temperature_with_offset);
    RUN_TEST(test_missing_assigned_sensor_reports_missing);
    RUN_TEST(test_missing_sensor_is_logged_once_per_loss);
    RUN_TEST(test_read_error_reports_read_error);
    RUN_TEST(test_restored_role_is_missing_until_sensor_found);
    RUN_TEST(test_sensor_missing_at_start_is_logged);
    RUN_TEST(test_bus_error_makes_assigned_role_missing);

    RUN_TEST(test_duplicate_id_gives_one_channel_marked_duplicate);
    RUN_TEST(test_duplicate_assigned_sensor_reports_duplicate);
    RUN_TEST(test_duplicate_is_logged_once_and_clears_when_resolved);
    RUN_TEST(test_nine_sensors_fill_eight_channels_and_report_overflow);
    RUN_TEST(test_assigned_sensor_has_priority_over_new_sensors);
    RUN_TEST(test_assigned_sensor_beyond_scan_limit_counts_as_missing);
    RUN_TEST(test_overflow_clears_when_sensors_are_removed);

    RUN_TEST(test_second_driver_type_works_without_core_changes);
    RUN_TEST(test_same_id_on_two_drivers_is_duplicate);
    RUN_TEST(test_bus_error_affects_only_its_own_driver);

    return UNITY_END();
}
