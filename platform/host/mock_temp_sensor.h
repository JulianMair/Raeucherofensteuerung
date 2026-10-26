/*
 * Mock eines Sensortreibers für Host-Tests. Er spielt einen Bus nach,
 * an dem der Test Sensoren an- und absteckt, Werte setzt und Fehler
 * einspeist.
 *
 * Doppelte IDs sind absichtlich erlaubt, damit die Sensorverwaltung
 * diesen Fall testen kann (SEN-07). Bei doppelten IDs wirken read,
 * set_temp und unplug auf den zuerst angesteckten Sensor.
 */
#ifndef MOCK_TEMP_SENSOR_H
#define MOCK_TEMP_SENSOR_H

#include "rof_temp_sensor.h"

#include <stdbool.h>

/* Mehr als die 8 Kanäle der Sensorverwaltung, damit der Überlauf testbar ist. */
#define MOCK_TEMP_SENSOR_MAX 16

typedef struct {
    bool plugged;
    rof_sensor_id_t id;
    const char *type;
    float temp_c;
    rof_status_t read_status; /* ROF_OK oder der eingespeiste Fehler für read */
} mock_temp_sensor_slot_t;

typedef struct {
    mock_temp_sensor_slot_t slots[MOCK_TEMP_SENSOR_MAX];
    rof_status_t enumerate_status; /* ROF_OK oder der eingespeiste Busfehler */
    unsigned read_calls;
} mock_temp_sensor_t;

void mock_temp_sensor_init(mock_temp_sensor_t *mock);

/* ROF_ERR_NO_SPACE, wenn alle MOCK_TEMP_SENSOR_MAX Plätze belegt sind. */
rof_status_t mock_temp_sensor_plug(mock_temp_sensor_t *mock, rof_sensor_id_t id,
                                   const char *type, float temp_c);

/* Die folgenden drei liefern ROF_ERR_NOT_FOUND, wenn die ID nicht steckt. */
rof_status_t mock_temp_sensor_unplug(mock_temp_sensor_t *mock, rof_sensor_id_t id);
rof_status_t mock_temp_sensor_set_temp(mock_temp_sensor_t *mock, rof_sensor_id_t id,
                                       float temp_c);
rof_status_t mock_temp_sensor_set_read_status(mock_temp_sensor_t *mock, rof_sensor_id_t id,
                                              rof_status_t status);

void mock_temp_sensor_set_enumerate_status(mock_temp_sensor_t *mock, rof_status_t status);

rof_temp_sensor_t mock_temp_sensor_interface(mock_temp_sensor_t *mock);

#endif /* MOCK_TEMP_SENSOR_H */
