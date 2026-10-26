/*
 * Sensorverwaltung (SEN-01 bis SEN-07).
 *
 * Verwaltet bis zu ROF_SENSOR_CHANNELS_MAX Kanäle. Ein Kanal steht für
 * einen Sensor mit ID, Typ und Rolle. Die Treiber melden über
 * enumerate, welche Sensoren gerade da sind; update() gleicht die
 * Kanäle damit ab.
 *
 * Der gesamte Speicher steckt im Struct rof_sensor_registry_t, das der
 * Aufrufer anlegt. Es gibt kein malloc.
 */
#ifndef ROF_SENSOR_REGISTRY_H
#define ROF_SENSOR_REGISTRY_H

#include "rof_log.h"
#include "rof_status.h"
#include "rof_storage.h"
#include "rof_temp_sensor.h"

#include <stdbool.h>
#include <stddef.h>

#define ROF_SENSOR_CHANNELS_MAX 8
#define ROF_SENSOR_DRIVERS_MAX 4
/* So viele Sensoren fragt update() je Treiber ab. Mehr als Kanäle, damit
 * zugeordnete Sensoren auch dann gefunden werden, wenn zu viele stecken. */
#define ROF_SENSOR_SCAN_MAX 16

/* Rollen (SEN-02). Die deutschen Begriffe stehen in docs/glossar.md. */
typedef enum {
    ROF_ROLE_UNASSIGNED = 0,
    ROF_ROLE_CHAMBER,
    ROF_ROLE_PROBE_1,
    ROF_ROLE_PROBE_2,
    ROF_ROLE_SMOKE_GEN,
    ROF_ROLE_COUNT /* Anzahl der Rollen, keine eigene Rolle */
} rof_role_t;

typedef struct {
    rof_sensor_id_t id;
    const char *type; /* vom Treiber gemeldeter Sensortyp */
    rof_role_t role;
    bool present;                    /* beim letzten update() gemeldet */
    const rof_temp_sensor_t *driver; /* Treiber, der den Sensor zuletzt gemeldet hat */
} rof_sensor_channel_t;

typedef struct {
    const rof_temp_sensor_t *drivers[ROF_SENSOR_DRIVERS_MAX];
    size_t driver_count;
    const rof_storage_t *storage;
    const rof_log_t *logger;
    rof_sensor_channel_t channels[ROF_SENSOR_CHANNELS_MAX];
    size_t channel_count;
    rof_sensor_info_t scan[ROF_SENSOR_SCAN_MAX]; /* Arbeitsspeicher für update() */
} rof_sensor_registry_t;

/* Prüft die Interfaces und setzt die Verwaltung auf.
 * Alle Interfaces müssen gültig bleiben, solange reg benutzt wird.
 * ROF_ERR_INVALID_ARG: ein Interface fehlt oder driver_count liegt nicht
 * zwischen 1 und ROF_SENSOR_DRIVERS_MAX. */
rof_status_t rof_sensor_registry_init(rof_sensor_registry_t *reg,
                                      const rof_temp_sensor_t *const *drivers,
                                      size_t driver_count, const rof_storage_t *storage,
                                      const rof_log_t *logger);

/* Fragt alle Treiber ab und gleicht die Kanäle ab. */
void rof_sensor_registry_update(rof_sensor_registry_t *reg);

size_t rof_sensor_registry_channel_count(const rof_sensor_registry_t *reg);

/* Kanal Nummer index oder NULL, wenn es ihn nicht gibt. Der Zeiger gilt
 * bis zum nächsten Aufruf, der die Verwaltung verändert. */
const rof_sensor_channel_t *rof_sensor_registry_channel(const rof_sensor_registry_t *reg,
                                                        size_t index);

#endif /* ROF_SENSOR_REGISTRY_H */
