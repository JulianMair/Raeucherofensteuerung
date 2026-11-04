#include "snapshot_store.h"

void snapshot_store_init(snapshot_store_t *store)
{
    store->mutex = xSemaphoreCreateMutexStatic(&store->mutex_buffer);
    store->has_snapshot = false;
    store->skipped = 0;
}

bool snapshot_store_publish(snapshot_store_t *store, const rof_snapshot_t *snapshot)
{
    if (xSemaphoreTake(store->mutex, 0) != pdTRUE) {
        store->skipped++;
        return false;
    }
    store->latest = *snapshot;
    store->has_snapshot = true;
    xSemaphoreGive(store->mutex);
    return true;
}

bool snapshot_store_read(snapshot_store_t *store, rof_snapshot_t *out, TickType_t wait)
{
    if (xSemaphoreTake(store->mutex, wait) != pdTRUE) {
        return false;
    }
    const bool available = store->has_snapshot;
    if (available) {
        *out = store->latest;
    }
    xSemaphoreGive(store->mutex);
    return available;
}
