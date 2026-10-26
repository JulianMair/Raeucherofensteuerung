/*
 * Temperatursensor-Interface (ARC-02, SEN-03, SEN-06).
 *
 * Eine Implementierung steht für einen Treiber, der mehrere Sensoren
 * verwaltet, z. B. alle DS18B20 an einem 1-Wire-Bus.
 */
#ifndef ROF_TEMP_SENSOR_H
#define ROF_TEMP_SENSOR_H

#include "rof_status.h"

#include <stddef.h>
#include <stdint.h>

/* Eindeutige ID eines Sensors, beim DS18B20 die 64-Bit-ROM-Adresse. */
typedef uint64_t rof_sensor_id_t;

typedef struct {
    rof_sensor_id_t id;
    /* Sensortyp als Text, z. B. "DS18B20". Zeigt auf einen konstanten String
     * des Treibers, der die ganze Laufzeit gültig bleibt. core wertet ihn
     * nicht aus, ein neuer Sensortyp braucht deshalb keine Änderung in core. */
    const char *type;
} rof_sensor_info_t;

typedef struct {
    void *ctx;
    /* Schreibt die gerade vorhandenen Sensoren nach list, höchstens max Stück,
     * und die Zahl der geschriebenen Einträge nach count.
     * ROF_ERR_NO_SPACE: Es sind mehr als max Sensoren vorhanden, list enthält die ersten max.
     * ROF_ERR_IO: Bus nicht lesbar, count ist 0. */
    rof_status_t (*enumerate)(void *ctx, rof_sensor_info_t *list, size_t max, size_t *count);
    /* Liefert den letzten Messwert des Sensors in °C.
     * ROF_ERR_NOT_FOUND: Sensor nicht (mehr) vorhanden.
     * ROF_ERR_IO: Messung fehlgeschlagen, z. B. CRC-Fehler.
     * Bei einem Fehler bleibt *temp_c unverändert. */
    rof_status_t (*read)(void *ctx, rof_sensor_id_t id, float *temp_c);
} rof_temp_sensor_t;

static inline rof_status_t rof_temp_sensor_enumerate(const rof_temp_sensor_t *sensor,
                                                     rof_sensor_info_t *list, size_t max,
                                                     size_t *count)
{
    return sensor->enumerate(sensor->ctx, list, max, count);
}

static inline rof_status_t rof_temp_sensor_read(const rof_temp_sensor_t *sensor,
                                                rof_sensor_id_t id, float *temp_c)
{
    return sensor->read(sensor->ctx, id, temp_c);
}

#endif /* ROF_TEMP_SENSOR_H */
