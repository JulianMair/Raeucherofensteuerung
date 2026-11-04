/*
 * Tests des DS18B20-Ablaufs aus platform/esp32 (SEN-03, SEN-06, E-032).
 * Ein Fake-Bus spielt die Sensoren: an- und abstecken, Messung, Lesefehler.
 */
#include "unity.h"

#include "ds18b20_sensors.h"
#include "fake_clock.h"

#include <string.h>

#define ID_A 0x28AA000000000001ULL
#define ID_B 0x28BB000000000002ULL
#define FAKE_MAX 20u

/* ---- Fake-Bus ------------------------------------------------------------ */

typedef struct {
    rof_sensor_id_t id;
    bool present;
    bool converted;    /* Scratchpad enthält eine Messung, sonst den Einschaltwert */
    bool bad_crc;      /* Störung auf dem Bus beim Lesen */
    uint16_t raw;      /* aktuelle Temperatur in 1/16 °C */
    uint16_t measured; /* Temperatur beim letzten Anstoßen, steht im Scratchpad */
} fake_sensor_t;

typedef struct {
    fake_sensor_t sensors[FAKE_MAX];
    size_t count;
    fake_clock_t *clock;
    bool search_fails;
    bool convert_fails;
    uint64_t convert_takes_ms;  /* so lange dauert der Befehl selbst */
    int plug_on_search;         /* dieser Sensor wird beim nächsten Suchen eingesteckt, -1 keiner */
    unsigned convert_calls;
    unsigned read_calls;
} fake_bus_t;

static fake_bus_t fake;
static fake_clock_t clock_fake;
static rof_clock_t clock_if;
static ds18b20_bus_t bus;
static ds18b20_sensors_t sensors;
static rof_temp_sensor_t driver;

static rof_status_t fake_search(void *ctx, rof_sensor_id_t *ids, size_t max, size_t *count)
{
    fake_bus_t *f = ctx;
    if (f->plug_on_search >= 0) {
        f->sensors[f->plug_on_search].present = true;
        f->plug_on_search = -1;
    }
    if (f->search_fails) {
        *count = 0;
        return ROF_ERR_IO;
    }
    size_t n = 0;
    bool overflow = false;
    for (size_t i = 0; i < f->count; i++) {
        if (!f->sensors[i].present) {
            continue;
        }
        if (n == max) {
            overflow = true;
            break;
        }
        ids[n++] = f->sensors[i].id;
    }
    *count = n;
    return overflow ? ROF_ERR_NO_SPACE : ROF_OK;
}

static rof_status_t fake_convert_all(void *ctx)
{
    fake_bus_t *f = ctx;
    f->convert_calls++;
    if (f->convert_fails) {
        return ROF_ERR_IO;
    }
    for (size_t i = 0; i < f->count; i++) {
        if (f->sensors[i].present) {
            f->sensors[i].converted = true;
            f->sensors[i].measured = f->sensors[i].raw;
        }
    }
    fake_clock_advance(f->clock, f->convert_takes_ms);
    return ROF_OK;
}

static rof_status_t fake_read_scratchpad(void *ctx, rof_sensor_id_t id, uint8_t sp[DS18B20_SCRATCHPAD_LEN])
{
    fake_bus_t *f = ctx;
    f->read_calls++;
    for (size_t i = 0; i < f->count; i++) {
        const fake_sensor_t *s = &f->sensors[i];
        if (s->id != id || !s->present) {
            continue;
        }
        /* Nach dem Einschalten steht 85 °C im Scratchpad (0x0550) */
        const uint16_t raw = s->converted ? s->measured : 0x0550u;
        const uint8_t bytes[DS18B20_SCRATCHPAD_LEN - 1] = {
            (uint8_t)(raw & 0xFFu), (uint8_t)(raw >> 8), 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10
        };
        memcpy(sp, bytes, sizeof(bytes));
        sp[DS18B20_SCRATCHPAD_LEN - 1] = ds18b20_crc8(sp, DS18B20_SCRATCHPAD_LEN - 1);
        if (s->bad_crc) {
            sp[0] ^= 0x01u;
        }
        return ROF_OK;
    }
    return ROF_ERR_IO; /* niemand antwortet */
}

static fake_sensor_t *plug(rof_sensor_id_t id, uint16_t raw)
{
    fake_sensor_t *s = &fake.sensors[fake.count++];
    *s = (fake_sensor_t){ .id = id, .present = true, .raw = raw };
    return s;
}

void setUp(void)
{
    memset(&fake, 0, sizeof(fake));
    fake.plug_on_search = -1;
    fake_clock_init(&clock_fake, 0);
    fake.clock = &clock_fake;
    clock_if = fake_clock_interface(&clock_fake);
    bus = (ds18b20_bus_t){ .ctx = &fake,
                           .search = fake_search,
                           .convert_all = fake_convert_all,
                           .read_scratchpad = fake_read_scratchpad };
    ds18b20_sensors_init(&sensors, &bus, &clock_if);
    driver = ds18b20_sensors_interface(&sensors);
}

void tearDown(void) {}

/* Eine Abfrage, dann eine Sekunde weiter, wie im Regeltask */
static void poll_and_wait(void)
{
    ds18b20_sensors_poll(&sensors);
    fake_clock_advance(&clock_fake, 1000);
}

static rof_status_t read_temp(rof_sensor_id_t id, float *temp_c)
{
    return rof_temp_sensor_read(&driver, id, temp_c);
}

static size_t enumerate_count(rof_status_t expected)
{
    rof_sensor_info_t list[DS18B20_SENSORS_MAX];
    size_t count = 99;
    TEST_ASSERT_EQUAL(expected, rof_temp_sensor_enumerate(&driver, list, DS18B20_SENSORS_MAX, &count));
    return count;
}

/* ---- Ablauf -------------------------------------------------------------- */

/* Erste Abfrage findet den Sensor, die zweite stößt die Messung an,
 * die dritte liest sie. Vorher gibt es keinen Wert. */
static void test_value_needs_search_conversion_and_read(void)
{
    plug(ID_A, 0x0158); /* 21,5 °C */
    float temp_c = -1.0f;

    poll_and_wait();
    TEST_ASSERT_EQUAL_size_t(1, enumerate_count(ROF_OK));
    TEST_ASSERT_EQUAL(ROF_ERR_IO, read_temp(ID_A, &temp_c));

    poll_and_wait();
    TEST_ASSERT_EQUAL(ROF_ERR_IO, read_temp(ID_A, &temp_c));

    poll_and_wait();
    TEST_ASSERT_EQUAL(ROF_OK, read_temp(ID_A, &temp_c));
    TEST_ASSERT_EQUAL_FLOAT(21.5f, temp_c);
}

static void test_enumerate_reports_type(void)
{
    plug(ID_A, 0x0158);
    poll_and_wait();
    rof_sensor_info_t list[2];
    size_t count = 0;
    TEST_ASSERT_EQUAL(ROF_OK, rof_temp_sensor_enumerate(&driver, list, 2, &count));
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_HEX64(ID_A, list[0].id);
    TEST_ASSERT_EQUAL_STRING("DS18B20", list[0].type);
}

static void test_values_follow_the_sensor(void)
{
    fake_sensor_t *a = plug(ID_A, 0x0158);
    poll_and_wait();
    poll_and_wait();
    poll_and_wait();
    a->raw = 0x0500; /* jetzt 80 °C */
    float temp_c = 0.0f;

    poll_and_wait(); /* liest die Messung, die vor der Änderung lief */
    TEST_ASSERT_EQUAL(ROF_OK, read_temp(ID_A, &temp_c));
    TEST_ASSERT_EQUAL_FLOAT(21.5f, temp_c);

    poll_and_wait();
    TEST_ASSERT_EQUAL(ROF_OK, read_temp(ID_A, &temp_c));
    TEST_ASSERT_EQUAL_FLOAT(80.0f, temp_c);
}

/* Ohne Sensoren wird keine Messung angestoßen */
static void test_no_conversion_on_empty_bus(void)
{
    poll_and_wait();
    poll_and_wait();
    TEST_ASSERT_EQUAL_UINT(0, fake.convert_calls);
    TEST_ASSERT_EQUAL_size_t(0, enumerate_count(ROF_OK));
}

/* ---- Zeit der Messung ---------------------------------------------------- */

/* Kommt die Abfrage zu früh, läuft die Messung weiter. Es wird nichts
 * gelesen und nichts neu angestoßen, die Werte bleiben. */
static void test_early_poll_keeps_conversion_and_values(void)
{
    plug(ID_A, 0x0158);
    poll_and_wait();
    poll_and_wait();
    poll_and_wait(); /* t = 3000: Wert da, Messung läuft seit t = 2000 */
    const unsigned converts = fake.convert_calls;
    const unsigned reads = fake.read_calls;

    ds18b20_sensors_poll(&sensors); /* t = 3000: fertig, lesen und neu anstoßen */
    fake_clock_advance(&clock_fake, 300);
    ds18b20_sensors_poll(&sensors); /* t = 3300: erst 300 ms, zu früh */

    TEST_ASSERT_EQUAL_UINT(reads + 1, fake.read_calls);
    TEST_ASSERT_EQUAL_UINT(converts + 1, fake.convert_calls);

    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_OK, read_temp(ID_A, &temp_c));
    TEST_ASSERT_EQUAL_FLOAT(21.5f, temp_c);
}

/* Die Messzeit zählt ab dem Ende des Befehls, nicht ab Beginn der Abfrage */
static void test_conversion_time_counts_from_command(void)
{
    plug(ID_A, 0x0158);
    fake.convert_takes_ms = 300;
    poll_and_wait();                 /* t = 0: Sensor gefunden */
    ds18b20_sensors_poll(&sensors);  /* t = 1000: Messung, Befehl endet bei 1300 */
    fake_clock_advance(&clock_fake, 2000 - 1300);
    ds18b20_sensors_poll(&sensors);  /* t = 2000: erst 700 ms, zu früh */
    TEST_ASSERT_EQUAL_UINT(0, fake.read_calls);

    fake_clock_advance(&clock_fake, 50); /* t = 2050: 750 ms */
    ds18b20_sensors_poll(&sensors);
    TEST_ASSERT_EQUAL_UINT(1, fake.read_calls);
    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_OK, read_temp(ID_A, &temp_c));
}

/* ---- An- und abstecken (SEN-03) ----------------------------------------- */

/* Ein Sensor, der nach dem Anstoßen eingesteckt wird, hat noch 85 °C im
 * Scratchpad. Dieser Wert darf nie als Messwert herauskommen (E-032). */
static void test_sensor_plugged_after_conversion_never_reports_power_on_value(void)
{
    plug(ID_A, 0x0158);
    fake_sensor_t *b = plug(ID_B, 0x0190); /* 25 °C */
    b->present = false;
    poll_and_wait();
    poll_and_wait();
    poll_and_wait();

    fake.plug_on_search = 1; /* B kommt nach dem Anstoßen der nächsten Messung */
    float temp_c = 0.0f;
    for (int i = 0; i < 4; i++) {
        poll_and_wait();
        if (read_temp(ID_B, &temp_c) == ROF_OK) {
            TEST_ASSERT_EQUAL_FLOAT(25.0f, temp_c);
        }
    }
    TEST_ASSERT_EQUAL(ROF_OK, read_temp(ID_B, &temp_c));
}

static void test_new_sensor_is_listed_before_it_has_a_value(void)
{
    plug(ID_A, 0x0158);
    poll_and_wait();
    poll_and_wait();
    poll_and_wait();
    plug(ID_B, 0x0190);
    poll_and_wait();

    TEST_ASSERT_EQUAL_size_t(2, enumerate_count(ROF_OK));
    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_IO, read_temp(ID_B, &temp_c));
    TEST_ASSERT_EQUAL(ROF_OK, read_temp(ID_A, &temp_c));
}

static void test_removed_sensor_is_not_found(void)
{
    plug(ID_A, 0x0158);
    fake_sensor_t *b = plug(ID_B, 0x0190);
    poll_and_wait();
    poll_and_wait();
    poll_and_wait();
    b->present = false;
    poll_and_wait();

    TEST_ASSERT_EQUAL_size_t(1, enumerate_count(ROF_OK));
    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, read_temp(ID_B, &temp_c));
    TEST_ASSERT_EQUAL(ROF_OK, read_temp(ID_A, &temp_c));
}

static void test_unknown_id_is_not_found(void)
{
    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, read_temp(ID_A, &temp_c));
}

/* ---- Fehler -------------------------------------------------------------- */

/* Ein Lesefehler betrifft nur diesen Sensor, und der alte Wert gilt nicht weiter */
static void test_read_error_hides_old_value(void)
{
    fake_sensor_t *a = plug(ID_A, 0x0158);
    plug(ID_B, 0x0190);
    poll_and_wait();
    poll_and_wait();
    poll_and_wait();
    a->bad_crc = true;
    poll_and_wait();

    float temp_c = 42.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_IO, read_temp(ID_A, &temp_c));
    TEST_ASSERT_EQUAL_FLOAT(42.0f, temp_c);
    TEST_ASSERT_EQUAL(ROF_OK, read_temp(ID_B, &temp_c));
}

/* Schlägt das Anstoßen fehl, gibt es bei der nächsten Abfrage keine Werte */
static void test_failed_conversion_leaves_no_values(void)
{
    plug(ID_A, 0x0158);
    poll_and_wait();
    poll_and_wait();
    poll_and_wait();
    fake.convert_fails = true;
    poll_and_wait(); /* liest noch, Anstoßen scheitert */
    poll_and_wait();

    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_IO, read_temp(ID_A, &temp_c));
}

static void test_bus_error_reports_io_and_no_sensors(void)
{
    plug(ID_A, 0x0158);
    poll_and_wait();
    fake.search_fails = true;
    poll_and_wait();

    TEST_ASSERT_EQUAL_size_t(0, enumerate_count(ROF_ERR_IO));
    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_NOT_FOUND, read_temp(ID_A, &temp_c));
}

/* Mehr Sensoren, als der Treiber fasst: die ersten werden gemeldet */
static void test_too_many_sensors_reports_no_space(void)
{
    for (size_t i = 0; i < DS18B20_SENSORS_MAX + 1; i++) {
        plug(0x2800000000000100ULL + i, 0x0158);
    }
    poll_and_wait();
    TEST_ASSERT_EQUAL_size_t(DS18B20_SENSORS_MAX, enumerate_count(ROF_ERR_NO_SPACE));
}

/* Fragt die Sensorverwaltung nach weniger, als da sind, ebenfalls */
static void test_enumerate_with_small_list_reports_no_space(void)
{
    plug(ID_A, 0x0158);
    plug(ID_B, 0x0190);
    poll_and_wait();
    rof_sensor_info_t list[1];
    size_t count = 0;
    TEST_ASSERT_EQUAL(ROF_ERR_NO_SPACE, rof_temp_sensor_enumerate(&driver, list, 1, &count));
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_HEX64(ID_A, list[0].id);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_value_needs_search_conversion_and_read);
    RUN_TEST(test_enumerate_reports_type);
    RUN_TEST(test_values_follow_the_sensor);
    RUN_TEST(test_no_conversion_on_empty_bus);
    RUN_TEST(test_early_poll_keeps_conversion_and_values);
    RUN_TEST(test_conversion_time_counts_from_command);
    RUN_TEST(test_sensor_plugged_after_conversion_never_reports_power_on_value);
    RUN_TEST(test_new_sensor_is_listed_before_it_has_a_value);
    RUN_TEST(test_removed_sensor_is_not_found);
    RUN_TEST(test_unknown_id_is_not_found);
    RUN_TEST(test_read_error_hides_old_value);
    RUN_TEST(test_failed_conversion_leaves_no_values);
    RUN_TEST(test_bus_error_reports_io_and_no_sensors);
    RUN_TEST(test_too_many_sensors_reports_no_space);
    RUN_TEST(test_enumerate_with_small_list_reports_no_space);
    return UNITY_END();
}
