/*
 * Speicher für core auf dem ESP32 (SEN-04, E-035): NVS im Flash, ein
 * Namensraum für das Projekt, je Schlüssel ein Datenblock.
 */
#ifndef ESP32_NVS_STORAGE_H
#define ESP32_NVS_STORAGE_H

#include "rof_storage.h"

#include "esp_err.h"
#include "nvs.h"

#include <stdbool.h>

#define ESP32_NVS_NAMESPACE "rof"
/* Größter Block, den load liest. Die Sensorzuordnung braucht höchstens 106 Byte (E-012). */
#define ESP32_NVS_BLOB_MAX 256u

typedef struct {
    nvs_handle_t handle;
    bool open; /* false: Einrichten fehlgeschlagen, jeder Zugriff liefert ROF_ERR_IO */
} esp32_nvs_storage_t;

/* Richtet NVS ein und öffnet den Namensraum. Ist die Partition voll oder
 * von einer neueren ESP-IDF-Version, wird sie gelöscht und *erased auf
 * true gesetzt: Die gespeicherte Zuordnung ist dann weg. Bei einem Fehler
 * läuft die Regelung ohne Zuordnung weiter (E-033). */
esp_err_t esp32_nvs_storage_init(esp32_nvs_storage_t *storage, bool *erased);

/* Das Interface für core. Der Zeiger muss gültig bleiben. */
rof_storage_t esp32_nvs_storage_interface(esp32_nvs_storage_t *storage);

#endif /* ESP32_NVS_STORAGE_H */
