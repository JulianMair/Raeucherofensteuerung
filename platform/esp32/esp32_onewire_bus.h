/*
 * 1-Wire-Bus auf dem ESP32 für die DS18B20 (SEN-03, E-032).
 *
 * Das Timing im Mikrosekundenbereich erzeugt die Komponente
 * espressif/onewire_bus mit der RMT-Hardware. Dieses Modul schickt nur die
 * Befehle aus dem Datenblatt. Ausgewertet wird in ds18b20_sensors.
 *
 * Voraussetzung: Die DS18B20 haben eine eigene Versorgung (VDD an 3,3 V),
 * und am Bus hängt ein Pull-up von 4,7 kΩ. Parasitäre Speisung geht nicht,
 * weil während einer Messung auf dem Bus weiter gesucht wird.
 */
#ifndef ESP32_ONEWIRE_BUS_H
#define ESP32_ONEWIRE_BUS_H

#include "ds18b20_sensors.h"

#include "esp_err.h"
#include "onewire_bus.h"

typedef struct {
    onewire_bus_handle_t handle; /* NULL, wenn der Bus nicht eingerichtet werden konnte */
} esp32_onewire_bus_t;

/* Richtet den Bus an gpio ein. Bei einem Fehler bleibt handle NULL, und
 * jeder Zugriff liefert ROF_ERR_IO. Die Regelung läuft dann ohne Sensoren
 * und meldet den fehlenden Garraumsensor (E-033). */
esp_err_t esp32_onewire_bus_init(esp32_onewire_bus_t *bus, int gpio);

/* Der Bus für ds18b20_sensors. Der Zeiger muss gültig bleiben. */
ds18b20_bus_t esp32_onewire_bus_interface(esp32_onewire_bus_t *bus);

#endif /* ESP32_ONEWIRE_BUS_H */
