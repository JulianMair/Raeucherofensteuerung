/*
 * Protokoll des DS18B20 (SEN-01, SEN-03, E-032): CRC, ROM-Adresse und
 * Auswertung des Scratchpads. Reines C ohne ESP-IDF, damit die Host-Tests
 * es prüfen können. Den Bus selbst bedient esp32_onewire_bus.
 */
#ifndef DS18B20_PROTOCOL_H
#define DS18B20_PROTOCOL_H

#include "rof_status.h"
#include "rof_temp_sensor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DS18B20_FAMILY_CODE 0x28u
#define DS18B20_ROM_LEN 8u         /* Familie, 6 Byte Seriennummer, CRC */
#define DS18B20_SCRATCHPAD_LEN 9u  /* 8 Byte Daten, dann die CRC */
#define DS18B20_CONVERSION_MS 750u /* längste Messzeit, bei 12 Bit */

/* Befehle aus dem Datenblatt */
#define DS18B20_CMD_SKIP_ROM 0xCCu        /* alle Sensoren zugleich ansprechen */
#define DS18B20_CMD_MATCH_ROM 0x55u       /* einen Sensor über seine Adresse ansprechen */
#define DS18B20_CMD_CONVERT_T 0x44u       /* Messung starten */
#define DS18B20_CMD_READ_SCRATCHPAD 0xBEu /* Scratchpad lesen */

/* CRC-8 nach Dallas/Maxim, Polynom x^8 + x^5 + x^4 + 1, Startwert 0 */
uint8_t ds18b20_crc8(const uint8_t *data, size_t len);

/* Macht aus einer ROM-Adresse in Bus-Reihenfolge die Sensor-ID, mit dem
 * Familiencode im höchsten Byte. false, und *id bleibt unverändert, bei
 * falscher CRC oder wenn der Baustein kein DS18B20 ist. */
bool ds18b20_rom_to_id(const uint8_t rom[DS18B20_ROM_LEN], rof_sensor_id_t *id);

/* Umkehrung: die ROM-Adresse in Bus-Reihenfolge, z. B. für MATCH ROM */
void ds18b20_id_to_rom(rof_sensor_id_t id, uint8_t rom[DS18B20_ROM_LEN]);

/* Wertet einen gelesenen Scratchpad aus und schreibt die Temperatur in °C
 * nach *temp_c. ROF_ERR_IO bei falscher CRC oder unmöglichem Inhalt,
 * *temp_c bleibt dann unverändert. Ob der Wert plausibel ist, prüft core
 * (SAF-04), nicht der Treiber. */
rof_status_t ds18b20_decode_scratchpad(const uint8_t sp[DS18B20_SCRATCHPAD_LEN], float *temp_c);

#endif /* DS18B20_PROTOCOL_H */
