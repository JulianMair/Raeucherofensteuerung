#include "rof_sensor_registry.h"

#include <string.h>

static bool driver_is_valid(const rof_temp_sensor_t *driver)
{
    return driver != NULL && driver->enumerate != NULL && driver->read != NULL;
}

static rof_sensor_channel_t *find_channel(rof_sensor_registry_t *reg, rof_sensor_id_t id)
{
    for (size_t i = 0; i < reg->channel_count; i++) {
        if (reg->channels[i].id == id) {
            return &reg->channels[i];
        }
    }
    return NULL;
}

/* Vermerkt einen vom Treiber gemeldeten Sensor. Unbekannte Sensoren
 * bekommen einen freien Kanal und sind zunächst nicht zugeordnet (SEN-03). */
static void note_sensor(rof_sensor_registry_t *reg, const rof_temp_sensor_t *driver,
                        const rof_sensor_info_t *info)
{
    rof_sensor_channel_t *ch = find_channel(reg, info->id);
    if (ch == NULL) {
        if (reg->channel_count == ROF_SENSOR_CHANNELS_MAX) {
            return;
        }
        ch = &reg->channels[reg->channel_count++];
        *ch = (rof_sensor_channel_t){ .id = info->id, .role = ROF_ROLE_UNASSIGNED };
        rof_log_write(reg->logger, ROF_LOG_INFO, "Neuer Sensor erkannt");
    }
    ch->present = true;
    ch->type = info->type;
    ch->driver = driver;
}

static void scan_driver(rof_sensor_registry_t *reg, const rof_temp_sensor_t *driver)
{
    size_t count = 0;
    rof_status_t status = rof_temp_sensor_enumerate(driver, reg->scan, ROF_SENSOR_SCAN_MAX, &count);
    if (status != ROF_OK) {
        /* Die Sensoren dieses Treibers gelten in diesem Durchlauf als nicht vorhanden. */
        rof_log_write(reg->logger, ROF_LOG_ERROR, "Sensorbus nicht lesbar");
        return;
    }
    for (size_t i = 0; i < count; i++) {
        note_sensor(reg, driver, &reg->scan[i]);
    }
}

static bool channel_in_use(const rof_sensor_channel_t *ch)
{
    return ch->present;
}

/* Entfernt Kanäle, die nicht mehr gebraucht werden. Die Reihenfolge der
 * übrigen bleibt erhalten. */
static void remove_unused_channels(rof_sensor_registry_t *reg)
{
    size_t kept = 0;
    for (size_t i = 0; i < reg->channel_count; i++) {
        if (channel_in_use(&reg->channels[i])) {
            reg->channels[kept++] = reg->channels[i];
        }
    }
    reg->channel_count = kept;
}

rof_status_t rof_sensor_registry_init(rof_sensor_registry_t *reg,
                                      const rof_temp_sensor_t *const *drivers,
                                      size_t driver_count, const rof_storage_t *storage,
                                      const rof_log_t *logger)
{
    if (reg == NULL || drivers == NULL || driver_count == 0 ||
        driver_count > ROF_SENSOR_DRIVERS_MAX || storage == NULL || storage->load == NULL ||
        storage->save == NULL || logger == NULL || logger->write == NULL) {
        return ROF_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < driver_count; i++) {
        if (!driver_is_valid(drivers[i])) {
            return ROF_ERR_INVALID_ARG;
        }
    }

    memset(reg, 0, sizeof(*reg));
    for (size_t i = 0; i < driver_count; i++) {
        reg->drivers[i] = drivers[i];
    }
    reg->driver_count = driver_count;
    reg->storage = storage;
    reg->logger = logger;
    return ROF_OK;
}

void rof_sensor_registry_update(rof_sensor_registry_t *reg)
{
    for (size_t i = 0; i < reg->channel_count; i++) {
        reg->channels[i].present = false;
    }
    for (size_t d = 0; d < reg->driver_count; d++) {
        scan_driver(reg, reg->drivers[d]);
    }
    remove_unused_channels(reg);
}

size_t rof_sensor_registry_channel_count(const rof_sensor_registry_t *reg)
{
    return reg->channel_count;
}

const rof_sensor_channel_t *rof_sensor_registry_channel(const rof_sensor_registry_t *reg,
                                                        size_t index)
{
    if (index >= reg->channel_count) {
        return NULL;
    }
    return &reg->channels[index];
}
