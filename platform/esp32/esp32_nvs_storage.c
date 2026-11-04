#include "esp32_nvs_storage.h"

#include "nvs_flash.h"

#include <stdint.h>
#include <string.h>

esp_err_t esp32_nvs_storage_init(esp32_nvs_storage_t *storage, bool *erased)
{
    storage->open = false;
    *erased = false;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Der Weg, den ESP-IDF dafür vorsieht: löschen und neu einrichten */
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            *erased = true;
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_open(ESP32_NVS_NAMESPACE, NVS_READWRITE, &storage->handle);
    storage->open = err == ESP_OK;
    return err;
}

static bool key_valid(const char *key)
{
    return key != NULL && key[0] != '\0' && strlen(key) <= ROF_STORAGE_KEY_MAX_LEN;
}

static rof_status_t storage_load(void *ctx, const char *key, void *buf, size_t buf_size, size_t *len)
{
    const esp32_nvs_storage_t *storage = ctx;
    if (!key_valid(key)) {
        return ROF_ERR_INVALID_ARG;
    }
    if (!storage->open) {
        return ROF_ERR_IO;
    }
    /* Erst nur die Größe abfragen */
    size_t size = 0;
    esp_err_t err = nvs_get_blob(storage->handle, key, NULL, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ROF_ERR_NOT_FOUND;
    }
    if (err != ESP_OK) {
        return ROF_ERR_IO;
    }
    if (size > buf_size || size > ESP32_NVS_BLOB_MAX) {
        return ROF_ERR_NO_SPACE;
    }
    /* Über eine Kopie lesen: Scheitert das Lesen mittendrin, bleibt buf
     * unverändert, wie es das Interface verspricht. */
    uint8_t copy[ESP32_NVS_BLOB_MAX];
    err = nvs_get_blob(storage->handle, key, copy, &size);
    if (err != ESP_OK) {
        return ROF_ERR_IO;
    }
    memcpy(buf, copy, size);
    *len = size;
    return ROF_OK;
}

static rof_status_t storage_save(void *ctx, const char *key, const void *data, size_t len)
{
    const esp32_nvs_storage_t *storage = ctx;
    if (!key_valid(key)) {
        return ROF_ERR_INVALID_ARG;
    }
    if (!storage->open) {
        return ROF_ERR_IO;
    }
    const esp_err_t err = nvs_set_blob(storage->handle, key, data, len);
    if (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
        return ROF_ERR_NO_SPACE;
    }
    if (err != ESP_OK) {
        return ROF_ERR_IO;
    }
    /* Erst nach nvs_commit ist der Block sicher im Flash */
    return nvs_commit(storage->handle) == ESP_OK ? ROF_OK : ROF_ERR_IO;
}

rof_storage_t esp32_nvs_storage_interface(esp32_nvs_storage_t *storage)
{
    return (rof_storage_t){ .ctx = storage, .load = storage_load, .save = storage_save };
}
