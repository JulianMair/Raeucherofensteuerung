/*
 * Tests des DS18B20-Protokolls aus platform/esp32 (SEN-01, SEN-03, E-032).
 * Reines C, deshalb auf dem Host prüfbar, obwohl es zur Firmware gehört.
 */
#include "unity.h"

#include "ds18b20_protocol.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Scratchpad wie vom Sensor: Rohwert, TH, TL, Konfiguration, drei
 * reservierte Bytes und die CRC über die ersten acht. */
static void make_scratchpad(uint16_t raw, uint8_t config, uint8_t sp[DS18B20_SCRATCHPAD_LEN])
{
    const uint8_t bytes[DS18B20_SCRATCHPAD_LEN - 1] = {
        (uint8_t)(raw & 0xFFu), (uint8_t)(raw >> 8), 0x4B, 0x46, config, 0xFF, 0x0C, 0x10
    };
    memcpy(sp, bytes, sizeof(bytes));
    sp[DS18B20_SCRATCHPAD_LEN - 1] = ds18b20_crc8(sp, DS18B20_SCRATCHPAD_LEN - 1);
}

static float decode_ok(uint16_t raw)
{
    uint8_t sp[DS18B20_SCRATCHPAD_LEN];
    make_scratchpad(raw, 0x7F, sp);
    float temp_c = -999.0f;
    TEST_ASSERT_EQUAL(ROF_OK, ds18b20_decode_scratchpad(sp, &temp_c));
    return temp_c;
}

/* ---- CRC ----------------------------------------------------------------- */

/* Beispiel aus Maxim Application Note 27: ROM 02 1C B8 01 00 00 00, CRC A2 */
static void test_crc8_matches_maxim_example(void)
{
    const uint8_t rom[] = { 0x02, 0x1C, 0xB8, 0x01, 0x00, 0x00, 0x00 };
    TEST_ASSERT_EQUAL_HEX8(0xA2, ds18b20_crc8(rom, sizeof(rom)));
}

/* Scratchpad eines DS18B20 direkt nach dem Einschalten, CRC 1C */
static void test_crc8_matches_power_on_scratchpad(void)
{
    const uint8_t sp[] = { 0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10 };
    TEST_ASSERT_EQUAL_HEX8(0x1C, ds18b20_crc8(sp, sizeof(sp)));
}

/* ---- ROM-Adresse und Sensor-ID ------------------------------------------ */

/* Der Familiencode steht vorn, so wie die ID in der API erscheint (E-028) */
static void test_rom_to_id_puts_family_code_first(void)
{
    const uint8_t rom[DS18B20_ROM_LEN] = { 0x28, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8D };
    rof_sensor_id_t id = 0;
    TEST_ASSERT_TRUE(ds18b20_rom_to_id(rom, &id));
    TEST_ASSERT_EQUAL_HEX64(0x28AA00000000008DULL, id);
}

static void test_id_to_rom_restores_bus_order(void)
{
    uint8_t rom[DS18B20_ROM_LEN];
    ds18b20_id_to_rom(0x28AA00000000008DULL, rom);
    const uint8_t expected[DS18B20_ROM_LEN] = { 0x28, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8D };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, rom, DS18B20_ROM_LEN);
}

static void test_rom_with_wrong_crc_is_rejected(void)
{
    const uint8_t rom[DS18B20_ROM_LEN] = { 0x28, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8C };
    rof_sensor_id_t id = 7;
    TEST_ASSERT_FALSE(ds18b20_rom_to_id(rom, &id));
    TEST_ASSERT_EQUAL_HEX64(7, id);
}

/* Andere 1-Wire-Bausteine am Bus, z. B. ein DS18S20 (Familie 10), zählen nicht */
static void test_rom_of_other_family_is_rejected(void)
{
    const uint8_t rom[DS18B20_ROM_LEN] = { 0x10, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68 };
    rof_sensor_id_t id = 7;
    TEST_ASSERT_FALSE(ds18b20_rom_to_id(rom, &id));
    TEST_ASSERT_EQUAL_HEX64(7, id);
}

/* ---- Scratchpad ---------------------------------------------------------- */

/* Werte aus dem Datenblatt, Tabelle 1 */
static void test_decode_values_from_datasheet(void)
{
    TEST_ASSERT_EQUAL_FLOAT(125.0f, decode_ok(0x07D0));
    TEST_ASSERT_EQUAL_FLOAT(85.0f, decode_ok(0x0550));
    TEST_ASSERT_EQUAL_FLOAT(25.0625f, decode_ok(0x0191));
    TEST_ASSERT_EQUAL_FLOAT(10.125f, decode_ok(0x00A2));
    TEST_ASSERT_EQUAL_FLOAT(0.5f, decode_ok(0x0008));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, decode_ok(0x0000));
    TEST_ASSERT_EQUAL_FLOAT(-0.5f, decode_ok(0xFFF8));
    TEST_ASSERT_EQUAL_FLOAT(-10.125f, decode_ok(0xFF5E));
    TEST_ASSERT_EQUAL_FLOAT(-25.0625f, decode_ok(0xFE6F));
    TEST_ASSERT_EQUAL_FLOAT(-55.0f, decode_ok(0xFC90));
}

/* Das Protokoll kann den Einschaltwert 85 °C nicht von echten 85 °C
 * unterscheiden. Das erledigt ds18b20_sensors über den Ablauf (E-032). */
static void test_power_on_scratchpad_decodes_to_85(void)
{
    const uint8_t sp[DS18B20_SCRATCHPAD_LEN] = { 0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0x1C };
    float temp_c = 0.0f;
    TEST_ASSERT_EQUAL(ROF_OK, ds18b20_decode_scratchpad(sp, &temp_c));
    TEST_ASSERT_EQUAL_FLOAT(85.0f, temp_c);
}

static void test_decode_rejects_wrong_crc(void)
{
    uint8_t sp[DS18B20_SCRATCHPAD_LEN];
    make_scratchpad(0x0191, 0x7F, sp);
    sp[0] ^= 0x01; /* ein gekipptes Bit auf dem Bus */
    float temp_c = 42.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_IO, ds18b20_decode_scratchpad(sp, &temp_c));
    TEST_ASSERT_EQUAL_FLOAT(42.0f, temp_c);
}

/* Datenleitung auf Masse: lauter Nullen. Deren CRC ist ebenfalls 0, nur
 * das Konfigurationsregister verrät den Fehler. */
static void test_decode_rejects_all_zero(void)
{
    const uint8_t sp[DS18B20_SCRATCHPAD_LEN] = { 0 };
    float temp_c = 42.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_IO, ds18b20_decode_scratchpad(sp, &temp_c));
    TEST_ASSERT_EQUAL_FLOAT(42.0f, temp_c);
}

/* Kein Sensor antwortet: Der Pull-up liefert lauter Einsen */
static void test_decode_rejects_all_ones(void)
{
    uint8_t sp[DS18B20_SCRATCHPAD_LEN];
    memset(sp, 0xFF, sizeof(sp));
    float temp_c = 42.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_IO, ds18b20_decode_scratchpad(sp, &temp_c));
    TEST_ASSERT_EQUAL_FLOAT(42.0f, temp_c);
}

/* Konfigurationsregister mit gesetztem Bit 7, CRC passt trotzdem */
static void test_decode_rejects_impossible_config(void)
{
    uint8_t sp[DS18B20_SCRATCHPAD_LEN];
    make_scratchpad(0x0191, 0xFF, sp);
    float temp_c = 42.0f;
    TEST_ASSERT_EQUAL(ROF_ERR_IO, ds18b20_decode_scratchpad(sp, &temp_c));
    TEST_ASSERT_EQUAL_FLOAT(42.0f, temp_c);
}

/* Bei 9 Bit Auflösung sind die drei untersten Bits undefiniert */
static void test_decode_ignores_undefined_bits_at_lower_resolution(void)
{
    uint8_t sp[DS18B20_SCRATCHPAD_LEN];
    float temp_c = 0.0f;

    make_scratchpad(0x0197, 0x1F, sp); /* 9 Bit */
    TEST_ASSERT_EQUAL(ROF_OK, ds18b20_decode_scratchpad(sp, &temp_c));
    TEST_ASSERT_EQUAL_FLOAT(25.0f, temp_c);

    make_scratchpad(0x0197, 0x5F, sp); /* 11 Bit: nur das unterste Bit */
    TEST_ASSERT_EQUAL(ROF_OK, ds18b20_decode_scratchpad(sp, &temp_c));
    TEST_ASSERT_EQUAL_FLOAT(25.375f, temp_c);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc8_matches_maxim_example);
    RUN_TEST(test_crc8_matches_power_on_scratchpad);
    RUN_TEST(test_rom_to_id_puts_family_code_first);
    RUN_TEST(test_id_to_rom_restores_bus_order);
    RUN_TEST(test_rom_with_wrong_crc_is_rejected);
    RUN_TEST(test_rom_of_other_family_is_rejected);
    RUN_TEST(test_decode_values_from_datasheet);
    RUN_TEST(test_power_on_scratchpad_decodes_to_85);
    RUN_TEST(test_decode_rejects_wrong_crc);
    RUN_TEST(test_decode_rejects_all_zero);
    RUN_TEST(test_decode_rejects_all_ones);
    RUN_TEST(test_decode_rejects_impossible_config);
    RUN_TEST(test_decode_ignores_undefined_bits_at_lower_resolution);
    return UNITY_END();
}
