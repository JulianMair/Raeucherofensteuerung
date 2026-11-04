/*
 * DS18B20 an einem 1-Wire-Bus als Temperatursensor für core (SEN-03,
 * SEN-06, E-032). Reines C: Den Bus liefert ds18b20_bus_t, auf dem ESP32
 * esp32_onewire_bus, in den Tests ein Fake.
 *
 * Eine Messung dauert bis zu 750 ms. Damit niemand so lange wartet, ruft
 * der Regeltask vor jedem Regelzyklus ds18b20_sensors_poll() auf:
 *   1. Messwerte lesen, wenn die zuletzt angestoßene Messung fertig ist
 *   2. neue Messung bei allen bekannten Sensoren anstoßen
 *   3. Bus nach Sensoren absuchen, während die Messung läuft
 * Die Messung läuft also zwischen zwei Abfragen. enumerate() und read()
 * geben nur zurück, was die letzte Abfrage ergeben hat.
 *
 * Ein Sensor liefert erst einen Wert, wenn er beim Anstoßen der Messung
 * schon bekannt war. Sonst stünde noch der Einschaltwert 85 °C in ihm.
 *
 * Versagt der Bus beim Lesen, liest die Abfrage nicht weiter und stößt
 * keine Messung an. Eine Abfrage greift dann höchstens zweimal auf den
 * gestörten Bus zu: einmal lesen, einmal suchen.
 *
 * Alle Funktionen nur aus einem Task aufrufen, in der Firmware dem Regeltask.
 */
#ifndef DS18B20_SENSORS_H
#define DS18B20_SENSORS_H

#include "ds18b20_protocol.h"
#include "rof_clock.h"
#include "rof_sensor_registry.h"
#include "rof_status.h"
#include "rof_temp_sensor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* So viele Sensoren merkt sich der Treiber, so viele fragt auch die
 * Sensorverwaltung beim Suchen ab. */
#define DS18B20_SENSORS_MAX ROF_SENSOR_SCAN_MAX

/* Zugriff auf den Bus. Jede Funktion kehrt zurück, ohne auf eine Messung
 * zu warten. */
typedef struct {
    void *ctx;
    /* Sucht alle DS18B20 am Bus und schreibt ihre IDs nach ids.
     * ROF_ERR_NO_SPACE: mehr als max, ids enthält die ersten max.
     * ROF_ERR_IO: Bus gestört, count ist 0. */
    rof_status_t (*search)(void *ctx, rof_sensor_id_t *ids, size_t max, size_t *count);
    /* Stößt bei allen Sensoren am Bus zugleich eine Messung an. */
    rof_status_t (*convert_all)(void *ctx);
    /* Liest den Scratchpad eines Sensors, ohne ihn auszuwerten. Fehlt nur
     * dieser Sensor, liefert der Bus lauter Einsen und ROF_OK.
     * ROF_ERR_IO: Der Bus selbst hat versagt, oder kein Sensor meldet sich. */
    rof_status_t (*read_scratchpad)(void *ctx, rof_sensor_id_t id, uint8_t sp[DS18B20_SCRATCHPAD_LEN]);
} ds18b20_bus_t;

typedef struct {
    rof_sensor_id_t id;
    bool in_conversion;  /* war bekannt, als die laufende Messung angestoßen wurde */
    rof_status_t status; /* ROF_OK, wenn temp_c aus der letzten Messung stammt */
    float temp_c;
} ds18b20_slot_t;

typedef struct {
    const ds18b20_bus_t *bus;
    const rof_clock_t *clk;
    ds18b20_slot_t slots[DS18B20_SENSORS_MAX];
    size_t count;
    rof_status_t search_status; /* Ergebnis der letzten Suche */
    bool converting;            /* eine Messung läuft */
    uint64_t conversion_ms;     /* Zeitpunkt, an dem sie angestoßen wurde */
} ds18b20_sensors_t;

void ds18b20_sensors_init(ds18b20_sensors_t *sensors, const ds18b20_bus_t *bus, const rof_clock_t *clk);

/* Eine Abfrage wie oben beschrieben. Kommt sie, bevor die laufende Messung
 * fertig ist, ändert sie nichts: Die Messung läuft weiter, und die Werte
 * der vorigen Abfrage bleiben gültig. */
void ds18b20_sensors_poll(ds18b20_sensors_t *sensors);

/* Das Interface für core. Der Zeiger muss gültig bleiben, solange es genutzt wird. */
rof_temp_sensor_t ds18b20_sensors_interface(ds18b20_sensors_t *sensors);

#endif /* DS18B20_SENSORS_H */
