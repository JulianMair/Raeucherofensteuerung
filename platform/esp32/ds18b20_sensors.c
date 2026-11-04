#include "ds18b20_sensors.h"

#include <string.h>

static const char *const SENSOR_TYPE = "DS18B20";

static ds18b20_slot_t *find_slot(ds18b20_sensors_t *sensors, rof_sensor_id_t id)
{
    for (size_t i = 0; i < sensors->count; i++) {
        if (sensors->slots[i].id == id) {
            return &sensors->slots[i];
        }
    }
    return NULL;
}

/* 1. Messwerte lesen. Ein Wert gilt nur bis zur nächsten Messung. */
static void read_values(ds18b20_sensors_t *sensors, bool conversion_done)
{
    for (size_t i = 0; i < sensors->count; i++) {
        ds18b20_slot_t *slot = &sensors->slots[i];
        slot->status = ROF_ERR_IO;
        if (!conversion_done || !slot->in_conversion) {
            continue;
        }
        uint8_t sp[DS18B20_SCRATCHPAD_LEN];
        float temp_c = 0.0f;
        if (sensors->bus->read_scratchpad(sensors->bus->ctx, slot->id, sp) == ROF_OK &&
            ds18b20_decode_scratchpad(sp, &temp_c) == ROF_OK) {
            slot->status = ROF_OK;
            slot->temp_c = temp_c;
        }
    }
}

/* 2. Neue Messung bei allen bekannten Sensoren anstoßen */
static void start_conversion(ds18b20_sensors_t *sensors)
{
    sensors->converting = false;
    for (size_t i = 0; i < sensors->count; i++) {
        sensors->slots[i].in_conversion = false;
    }
    if (sensors->count == 0 || sensors->bus->convert_all(sensors->bus->ctx) != ROF_OK) {
        return;
    }
    /* Die Zeit erst nach dem Befehl nehmen: Ab dann läuft die Messung sicher */
    sensors->converting = true;
    sensors->conversion_ms = rof_clock_now_ms(sensors->clk);
    for (size_t i = 0; i < sensors->count; i++) {
        sensors->slots[i].in_conversion = true;
    }
}

/* 3. Bus absuchen. Wer wieder gefunden wird, behält Wert und Messung.
 * Neue Sensoren haben noch keinen Wert, Verschwundene fallen heraus. */
static void search_bus(ds18b20_sensors_t *sensors)
{
    rof_sensor_id_t ids[DS18B20_SENSORS_MAX];
    size_t found = 0;
    sensors->search_status = sensors->bus->search(sensors->bus->ctx, ids, DS18B20_SENSORS_MAX, &found);
    if (sensors->search_status == ROF_ERR_IO || found > DS18B20_SENSORS_MAX) {
        found = 0;
    }

    ds18b20_slot_t next[DS18B20_SENSORS_MAX] = { 0 };
    for (size_t i = 0; i < found; i++) {
        const ds18b20_slot_t *old = find_slot(sensors, ids[i]);
        if (old != NULL) {
            next[i] = *old;
        } else {
            next[i] = (ds18b20_slot_t){ .id = ids[i], .in_conversion = false, .status = ROF_ERR_IO };
        }
    }
    memcpy(sensors->slots, next, found * sizeof(next[0]));
    sensors->count = found;
}

void ds18b20_sensors_init(ds18b20_sensors_t *sensors, const ds18b20_bus_t *bus, const rof_clock_t *clk)
{
    memset(sensors, 0, sizeof(*sensors));
    sensors->bus = bus;
    sensors->clk = clk;
    sensors->search_status = ROF_OK;
}

void ds18b20_sensors_poll(ds18b20_sensors_t *sensors)
{
    const uint64_t now_ms = rof_clock_now_ms(sensors->clk);
    if (sensors->converting && now_ms - sensors->conversion_ms < DS18B20_CONVERSION_MS) {
        return; /* zu früh, die Messung läuft noch */
    }
    read_values(sensors, sensors->converting);
    start_conversion(sensors);
    search_bus(sensors);
}

/* ---- Interface für core -------------------------------------------------- */

static rof_status_t sensors_enumerate(void *ctx, rof_sensor_info_t *list, size_t max, size_t *count)
{
    const ds18b20_sensors_t *sensors = ctx;
    if (sensors->search_status == ROF_ERR_IO) {
        *count = 0;
        return ROF_ERR_IO;
    }
    size_t n = 0;
    while (n < sensors->count && n < max) {
        list[n] = (rof_sensor_info_t){ .id = sensors->slots[n].id, .type = SENSOR_TYPE };
        n++;
    }
    *count = n;
    if (sensors->count > max || sensors->search_status == ROF_ERR_NO_SPACE) {
        return ROF_ERR_NO_SPACE;
    }
    return ROF_OK;
}

static rof_status_t sensors_read(void *ctx, rof_sensor_id_t id, float *temp_c)
{
    const ds18b20_slot_t *slot = find_slot(ctx, id);
    if (slot == NULL) {
        return ROF_ERR_NOT_FOUND;
    }
    if (slot->status != ROF_OK) {
        return slot->status;
    }
    *temp_c = slot->temp_c;
    return ROF_OK;
}

rof_temp_sensor_t ds18b20_sensors_interface(ds18b20_sensors_t *sensors)
{
    return (rof_temp_sensor_t){ .ctx = sensors, .enumerate = sensors_enumerate, .read = sensors_read };
}
