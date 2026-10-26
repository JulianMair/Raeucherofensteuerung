/*
 * Speicher-Interface (ARC-02, SEN-04): dauerhafte Ablage kleiner
 * Datenblöcke unter einem Schlüssel. Auf dem ESP32 steckt NVS dahinter.
 */
#ifndef ROF_STORAGE_H
#define ROF_STORAGE_H

#include "rof_status.h"

#include <stddef.h>

/* Längster erlaubter Schlüssel ohne abschließende Null, die Grenze von NVS. */
#define ROF_STORAGE_KEY_MAX_LEN 15

typedef struct {
    void *ctx;
    /* Lädt den Block zu key nach buf und schreibt seine Länge nach len.
     * ROF_ERR_NOT_FOUND: unter key wurde nie gespeichert, z. B. beim ersten Start.
     * ROF_ERR_NO_SPACE: Der Block ist größer als buf_size.
     * ROF_ERR_INVALID_ARG: key ist leer oder länger als ROF_STORAGE_KEY_MAX_LEN.
     * ROF_ERR_IO: Speicher nicht lesbar.
     * Bei einem Fehler bleiben buf und len unverändert. */
    rof_status_t (*load)(void *ctx, const char *key, void *buf, size_t buf_size, size_t *len);
    /* Speichert len Bytes aus data unter key und ersetzt einen alten Block.
     * ROF_ERR_NO_SPACE: kein Platz mehr.
     * ROF_ERR_INVALID_ARG: key ist leer oder zu lang.
     * ROF_ERR_IO: Schreiben fehlgeschlagen. */
    rof_status_t (*save)(void *ctx, const char *key, const void *data, size_t len);
} rof_storage_t;

static inline rof_status_t rof_storage_load(const rof_storage_t *storage, const char *key,
                                            void *buf, size_t buf_size, size_t *len)
{
    return storage->load(storage->ctx, key, buf, buf_size, len);
}

static inline rof_status_t rof_storage_save(const rof_storage_t *storage, const char *key,
                                            const void *data, size_t len)
{
    return storage->save(storage->ctx, key, data, len);
}

#endif /* ROF_STORAGE_H */
