#include "esp32_onewire_bus.h"

#include "esp_log.h"
#include "onewire_device.h"

#include <string.h>

/* Mehr Bausteine je Suche gelten als gestörter Bus. Andere 1-Wire-Bausteine
 * zählen mit, deshalb mehr als DS18B20_SENSORS_MAX. */
#define SEARCH_LIMIT (2u * DS18B20_SENSORS_MAX)

esp_err_t esp32_onewire_bus_init(esp32_onewire_bus_t *bus, int gpio)
{
    bus->handle = NULL;
    /* Der interne Pull-up ist zu schwach für den Bus, er ergänzt nur den
     * externen von 4,7 kΩ. */
    const onewire_bus_config_t config = { .bus_gpio_num = gpio, .flags = { .en_pull_up = true } };
    /* Der längste Empfang ist ein Scratchpad */
    const onewire_bus_rmt_config_t rmt_config = { .max_rx_bytes = DS18B20_SCRATCHPAD_LEN };
    /* Ein leerer Bus ist kein Fehler. Die Komponente meldet ihn sonst bei
     * jeder Suche, also jede Sekunde, als Warnung. */
    esp_log_level_set("1-wire.device", ESP_LOG_ERROR);
    return onewire_new_bus_rmt(&config, &rmt_config, &bus->handle);
}

static rof_status_t bus_search(void *ctx, rof_sensor_id_t *ids, size_t max, size_t *count)
{
    const esp32_onewire_bus_t *bus = ctx;
    *count = 0;
    onewire_device_iter_handle_t iter = NULL;
    if (bus->handle == NULL || onewire_new_device_iter(bus->handle, &iter) != ESP_OK) {
        return ROF_ERR_IO;
    }
    rof_status_t status = ROF_ERR_IO; /* gilt, wenn SEARCH_LIMIT erreicht wird */
    for (size_t found = 0; found < SEARCH_LIMIT; found++) {
        onewire_device_t device;
        const esp_err_t err = onewire_device_iter_get_next(iter, &device);
        if (err == ESP_ERR_NOT_FOUND) {
            status = ROF_OK; /* Suche beendet, auch bei leerem Bus */
            break;
        }
        if (err != ESP_OK) {
            break; /* z. B. falsche CRC einer Adresse */
        }
        /* address enthält die ROM-Bytes in Bus-Reihenfolge */
        uint8_t rom[DS18B20_ROM_LEN];
        memcpy(rom, &device.address, sizeof(rom));
        rof_sensor_id_t id;
        if (!ds18b20_rom_to_id(rom, &id)) {
            continue; /* anderer 1-Wire-Baustein */
        }
        if (*count == max) {
            status = ROF_ERR_NO_SPACE;
            break;
        }
        ids[(*count)++] = id;
    }
    onewire_del_device_iter(iter);
    if (status == ROF_ERR_IO) {
        *count = 0;
    }
    return status;
}

static rof_status_t bus_convert_all(void *ctx)
{
    const esp32_onewire_bus_t *bus = ctx;
    if (bus->handle == NULL || onewire_bus_reset(bus->handle) != ESP_OK) {
        return ROF_ERR_IO;
    }
    const uint8_t cmd[] = { DS18B20_CMD_SKIP_ROM, DS18B20_CMD_CONVERT_T };
    return onewire_bus_write_bytes(bus->handle, cmd, sizeof(cmd)) == ESP_OK ? ROF_OK : ROF_ERR_IO;
}

static rof_status_t bus_read_scratchpad(void *ctx, rof_sensor_id_t id, uint8_t sp[DS18B20_SCRATCHPAD_LEN])
{
    const esp32_onewire_bus_t *bus = ctx;
    if (bus->handle == NULL || onewire_bus_reset(bus->handle) != ESP_OK) {
        return ROF_ERR_IO;
    }
    /* MATCH ROM mit der Adresse, dann READ SCRATCHPAD */
    uint8_t cmd[1 + DS18B20_ROM_LEN + 1];
    cmd[0] = DS18B20_CMD_MATCH_ROM;
    ds18b20_id_to_rom(id, &cmd[1]);
    cmd[1 + DS18B20_ROM_LEN] = DS18B20_CMD_READ_SCRATCHPAD;
    if (onewire_bus_write_bytes(bus->handle, cmd, sizeof(cmd)) != ESP_OK) {
        return ROF_ERR_IO;
    }
    return onewire_bus_read_bytes(bus->handle, sp, DS18B20_SCRATCHPAD_LEN) == ESP_OK ? ROF_OK : ROF_ERR_IO;
}

ds18b20_bus_t esp32_onewire_bus_interface(esp32_onewire_bus_t *bus)
{
    return (ds18b20_bus_t){ .ctx = bus,
                            .search = bus_search,
                            .convert_all = bus_convert_all,
                            .read_scratchpad = bus_read_scratchpad };
}
