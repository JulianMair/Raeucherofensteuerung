#include "ds18b20_protocol.h"

/* Konfigurationsregister (Byte 4): Bit 7 ist immer 0, die Bits 0 bis 4
 * sind immer 1. Die Bits 5 und 6 geben die Auflösung an. */
#define DS18B20_CONFIG_FIXED_MASK 0x9Fu
#define DS18B20_CONFIG_FIXED_BITS 0x1Fu

uint8_t ds18b20_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        /* Bit für Bit, das niedrigste zuerst, wie auf dem Bus.
         * 0x8C ist das Polynom in umgekehrter Bitfolge. */
        for (int bit = 0; bit < 8; bit++) {
            const bool mix = ((crc ^ byte) & 0x01u) != 0;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8Cu;
            }
            byte >>= 1;
        }
    }
    return crc;
}

bool ds18b20_rom_to_id(const uint8_t rom[DS18B20_ROM_LEN], rof_sensor_id_t *id)
{
    if (ds18b20_crc8(rom, DS18B20_ROM_LEN - 1) != rom[DS18B20_ROM_LEN - 1]) {
        return false;
    }
    if (rom[0] != DS18B20_FAMILY_CODE) {
        return false;
    }
    /* Das erste Byte auf dem Bus (Familie) wird das höchste der ID */
    rof_sensor_id_t value = 0;
    for (size_t i = 0; i < DS18B20_ROM_LEN; i++) {
        value = (value << 8) | rom[i];
    }
    *id = value;
    return true;
}

void ds18b20_id_to_rom(rof_sensor_id_t id, uint8_t rom[DS18B20_ROM_LEN])
{
    for (size_t i = 0; i < DS18B20_ROM_LEN; i++) {
        rom[DS18B20_ROM_LEN - 1 - i] = (uint8_t)(id & 0xFFu);
        id >>= 8;
    }
}

rof_status_t ds18b20_decode_scratchpad(const uint8_t sp[DS18B20_SCRATCHPAD_LEN], float *temp_c)
{
    if (ds18b20_crc8(sp, DS18B20_SCRATCHPAD_LEN - 1) != sp[DS18B20_SCRATCHPAD_LEN - 1]) {
        return ROF_ERR_IO;
    }
    /* Lauter Nullen (Datenleitung auf Masse) haben eine gültige CRC und
     * fallen erst hier auf, lauter Einsen ebenfalls. */
    const uint8_t config = sp[4];
    if ((config & DS18B20_CONFIG_FIXED_MASK) != DS18B20_CONFIG_FIXED_BITS) {
        return ROF_ERR_IO;
    }
    /* Unter 12 Bit Auflösung sind die untersten Bits undefiniert */
    const unsigned resolution = 9u + ((config >> 5) & 0x03u);
    const unsigned undefined_bits = (1u << (12u - resolution)) - 1u;
    const unsigned raw = (((unsigned)sp[1] << 8) | sp[0]) & ~undefined_bits & 0xFFFFu;
    /* Zweierkomplement von Hand: Wie Werte über 32767 in int16_t umgewandelt
     * werden, legt C nicht fest. */
    const int32_t value = raw >= 0x8000u ? (int32_t)raw - 0x10000 : (int32_t)raw;
    *temp_c = (float)value / 16.0f; /* 1/16 °C je Stufe */
    return ROF_OK;
}
