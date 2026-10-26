#include "mock_storage.h"

#include <string.h>

static bool key_is_valid(const char *key)
{
    size_t len = strlen(key);
    return len > 0 && len <= ROF_STORAGE_KEY_MAX_LEN;
}

static mock_storage_slot_t *find_slot(mock_storage_t *mock, const char *key)
{
    for (size_t i = 0; i < MOCK_STORAGE_SLOTS; i++) {
        if (mock->slots[i].used && strcmp(mock->slots[i].key, key) == 0) {
            return &mock->slots[i];
        }
    }
    return NULL;
}

static mock_storage_slot_t *find_free_slot(mock_storage_t *mock)
{
    for (size_t i = 0; i < MOCK_STORAGE_SLOTS; i++) {
        if (!mock->slots[i].used) {
            return &mock->slots[i];
        }
    }
    return NULL;
}

static rof_status_t mock_load(void *ctx, const char *key, void *buf, size_t buf_size, size_t *len)
{
    mock_storage_t *mock = ctx;
    if (!key_is_valid(key)) {
        return ROF_ERR_INVALID_ARG;
    }
    if (mock->load_status != ROF_OK) {
        return mock->load_status;
    }
    const mock_storage_slot_t *slot = find_slot(mock, key);
    if (slot == NULL) {
        return ROF_ERR_NOT_FOUND;
    }
    if (slot->len > buf_size) {
        return ROF_ERR_NO_SPACE;
    }
    memcpy(buf, slot->data, slot->len);
    *len = slot->len;
    return ROF_OK;
}

static rof_status_t mock_save(void *ctx, const char *key, const void *data, size_t len)
{
    mock_storage_t *mock = ctx;
    mock->save_calls++;
    if (!key_is_valid(key)) {
        return ROF_ERR_INVALID_ARG;
    }
    if (mock->save_status != ROF_OK) {
        return mock->save_status;
    }
    if (len > MOCK_STORAGE_VALUE_MAX) {
        return ROF_ERR_NO_SPACE;
    }
    mock_storage_slot_t *slot = find_slot(mock, key);
    if (slot == NULL) {
        slot = find_free_slot(mock);
        if (slot == NULL) {
            return ROF_ERR_NO_SPACE;
        }
        slot->used = true;
        strcpy(slot->key, key);
    }
    memcpy(slot->data, data, len);
    slot->len = len;
    return ROF_OK;
}

void mock_storage_init(mock_storage_t *mock)
{
    memset(mock, 0, sizeof(*mock));
    mock->load_status = ROF_OK;
    mock->save_status = ROF_OK;
}

void mock_storage_set_load_status(mock_storage_t *mock, rof_status_t status)
{
    mock->load_status = status;
}

void mock_storage_set_save_status(mock_storage_t *mock, rof_status_t status)
{
    mock->save_status = status;
}

rof_storage_t mock_storage_interface(mock_storage_t *mock)
{
    rof_storage_t storage = { .ctx = mock, .load = mock_load, .save = mock_save };
    return storage;
}
