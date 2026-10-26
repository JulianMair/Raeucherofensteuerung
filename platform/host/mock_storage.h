/*
 * Speicher-Mock für Host-Tests: Schlüssel und Datenblöcke im RAM.
 *
 * Ein Neustart lässt sich nachstellen, indem der Test die core-Objekte
 * neu anlegt und diesen Mock behält. Die Daten überleben dann wie im Flash.
 */
#ifndef MOCK_STORAGE_H
#define MOCK_STORAGE_H

#include "rof_storage.h"

#include <stdbool.h>
#include <stdint.h>

#define MOCK_STORAGE_SLOTS 8
#define MOCK_STORAGE_VALUE_MAX 256

typedef struct {
    bool used;
    char key[ROF_STORAGE_KEY_MAX_LEN + 1];
    uint8_t data[MOCK_STORAGE_VALUE_MAX];
    size_t len;
} mock_storage_slot_t;

typedef struct {
    mock_storage_slot_t slots[MOCK_STORAGE_SLOTS];
    rof_status_t load_status; /* ROF_OK oder der eingespeiste Fehler für load */
    rof_status_t save_status; /* ROF_OK oder der eingespeiste Fehler für save */
    unsigned save_calls;
} mock_storage_t;

void mock_storage_init(mock_storage_t *mock);
void mock_storage_set_load_status(mock_storage_t *mock, rof_status_t status);
void mock_storage_set_save_status(mock_storage_t *mock, rof_status_t status);
rof_storage_t mock_storage_interface(mock_storage_t *mock);

#endif /* MOCK_STORAGE_H */
