#include "mock_temp_sensor.h"

#include <string.h>

/* Erster angesteckter Platz mit dieser ID, sonst NULL. */
static mock_temp_sensor_slot_t *find_plugged(mock_temp_sensor_t *mock, rof_sensor_id_t id)
{
    for (size_t i = 0; i < MOCK_TEMP_SENSOR_MAX; i++) {
        if (mock->slots[i].plugged && mock->slots[i].id == id) {
            return &mock->slots[i];
        }
    }
    return NULL;
}

static rof_status_t mock_enumerate(void *ctx, rof_sensor_info_t *list, size_t max, size_t *count)
{
    mock_temp_sensor_t *mock = ctx;
    *count = 0;
    if (mock->enumerate_status != ROF_OK) {
        return mock->enumerate_status;
    }
    for (size_t i = 0; i < MOCK_TEMP_SENSOR_MAX; i++) {
        if (!mock->slots[i].plugged) {
            continue;
        }
        if (*count == max) {
            return ROF_ERR_NO_SPACE;
        }
        list[*count].id = mock->slots[i].id;
        list[*count].type = mock->slots[i].type;
        (*count)++;
    }
    return ROF_OK;
}

static rof_status_t mock_read(void *ctx, rof_sensor_id_t id, float *temp_c)
{
    mock_temp_sensor_t *mock = ctx;
    mock->read_calls++;
    const mock_temp_sensor_slot_t *slot = find_plugged(mock, id);
    if (slot == NULL) {
        return ROF_ERR_NOT_FOUND;
    }
    if (slot->read_status != ROF_OK) {
        return slot->read_status;
    }
    *temp_c = slot->temp_c;
    return ROF_OK;
}

void mock_temp_sensor_init(mock_temp_sensor_t *mock)
{
    memset(mock, 0, sizeof(*mock));
    mock->enumerate_status = ROF_OK;
}

rof_status_t mock_temp_sensor_plug(mock_temp_sensor_t *mock, rof_sensor_id_t id,
                                   const char *type, float temp_c)
{
    for (size_t i = 0; i < MOCK_TEMP_SENSOR_MAX; i++) {
        mock_temp_sensor_slot_t *slot = &mock->slots[i];
        if (!slot->plugged) {
            slot->plugged = true;
            slot->id = id;
            slot->type = type;
            slot->temp_c = temp_c;
            slot->read_status = ROF_OK;
            return ROF_OK;
        }
    }
    return ROF_ERR_NO_SPACE;
}

rof_status_t mock_temp_sensor_unplug(mock_temp_sensor_t *mock, rof_sensor_id_t id)
{
    mock_temp_sensor_slot_t *slot = find_plugged(mock, id);
    if (slot == NULL) {
        return ROF_ERR_NOT_FOUND;
    }
    slot->plugged = false;
    return ROF_OK;
}

rof_status_t mock_temp_sensor_set_temp(mock_temp_sensor_t *mock, rof_sensor_id_t id,
                                       float temp_c)
{
    mock_temp_sensor_slot_t *slot = find_plugged(mock, id);
    if (slot == NULL) {
        return ROF_ERR_NOT_FOUND;
    }
    slot->temp_c = temp_c;
    return ROF_OK;
}

rof_status_t mock_temp_sensor_set_read_status(mock_temp_sensor_t *mock, rof_sensor_id_t id,
                                              rof_status_t status)
{
    mock_temp_sensor_slot_t *slot = find_plugged(mock, id);
    if (slot == NULL) {
        return ROF_ERR_NOT_FOUND;
    }
    slot->read_status = status;
    return ROF_OK;
}

void mock_temp_sensor_set_enumerate_status(mock_temp_sensor_t *mock, rof_status_t status)
{
    mock->enumerate_status = status;
}

rof_temp_sensor_t mock_temp_sensor_interface(mock_temp_sensor_t *mock)
{
    rof_temp_sensor_t sensor = { .ctx = mock, .enumerate = mock_enumerate, .read = mock_read };
    return sensor;
}
