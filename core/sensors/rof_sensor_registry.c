#include "rof_sensor_registry.h"

#include <stdint.h>
#include <string.h>

/*
 * Speicherformat Version 1 (E-012), Byte für Byte statt Struct-Kopie:
 *   Byte 0          Version
 *   Byte 1          Anzahl n der Einträge
 *   je Eintrag 13 Byte:
 *     ID     8 Byte, niederwertigstes Byte zuerst
 *     Rolle  1 Byte
 *     Offset 4 Byte, Bitmuster des float, niederwertigstes Byte zuerst
 */
#define STORE_VERSION 1u
#define STORE_HEADER_SIZE 2u
#define STORE_ENTRY_SIZE 13u
#define STORE_MAX_SIZE (STORE_HEADER_SIZE + ROF_SENSOR_CHANNELS_MAX * STORE_ENTRY_SIZE)

_Static_assert(sizeof(float) == 4, "Das Speicherformat setzt einen 32-Bit-float voraus");

static void put_u64(uint8_t *dst, uint64_t value)
{
    for (unsigned i = 0; i < 8; i++) {
        dst[i] = (uint8_t)(value >> (8u * i));
    }
}

static void put_f32(uint8_t *dst, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    for (unsigned i = 0; i < 4; i++) {
        dst[i] = (uint8_t)(bits >> (8u * i));
    }
}

static uint64_t get_u64(const uint8_t *src)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; i++) {
        value |= (uint64_t)src[i] << (8u * i);
    }
    return value;
}

static float get_f32(const uint8_t *src)
{
    uint32_t bits = 0;
    for (unsigned i = 0; i < 4; i++) {
        bits |= (uint32_t)src[i] << (8u * i);
    }
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

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

static rof_sensor_channel_t *find_role(rof_sensor_registry_t *reg, rof_role_t role)
{
    for (size_t i = 0; i < reg->channel_count; i++) {
        if (reg->channels[i].role == role) {
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

static void read_channel(rof_sensor_channel_t *ch)
{
    if (!ch->present) {
        ch->read_status = ROF_ERR_NOT_FOUND;
        return;
    }
    float raw_c = 0.0f;
    ch->read_status = rof_temp_sensor_read(ch->driver, ch->id, &raw_c);
    if (ch->read_status == ROF_OK) {
        ch->raw_c = raw_c;
        ch->temp_c = raw_c + ch->offset_c;
    }
}

/* So formuliert, dass auch NaN abgelehnt wird. */
static bool offset_is_valid(float offset_c)
{
    return offset_c >= -ROF_SENSOR_OFFSET_MAX_C && offset_c <= ROF_SENSOR_OFFSET_MAX_C;
}

static bool role_is_valid(rof_role_t role)
{
    return (unsigned)role < (unsigned)ROF_ROLE_COUNT;
}

/* Rolle oder Offset: Nur solche Kanäle werden gespeichert. */
static bool channel_has_settings(const rof_sensor_channel_t *ch)
{
    return ch->role != ROF_ROLE_UNASSIGNED || ch->offset_c != 0.0f;
}

/* Ein Kanal bleibt, solange der Sensor steckt oder Einstellungen trägt. */
static bool channel_in_use(const rof_sensor_channel_t *ch)
{
    return ch->present || channel_has_settings(ch);
}

static rof_status_t save_settings(const rof_sensor_registry_t *reg)
{
    uint8_t buf[STORE_MAX_SIZE];
    size_t len = STORE_HEADER_SIZE;
    uint8_t count = 0;
    for (size_t i = 0; i < reg->channel_count; i++) {
        const rof_sensor_channel_t *ch = &reg->channels[i];
        if (!channel_has_settings(ch)) {
            continue;
        }
        put_u64(&buf[len], ch->id);
        buf[len + 8] = (uint8_t)ch->role;
        put_f32(&buf[len + 9], ch->offset_c);
        len += STORE_ENTRY_SIZE;
        count++;
    }
    buf[0] = STORE_VERSION;
    buf[1] = count;
    return rof_storage_save(reg->storage, ROF_SENSOR_STORAGE_KEY, buf, len);
}

/* Legt für jeden gespeicherten Eintrag einen Kanal an. Liefert false,
 * sobald irgendetwas nicht stimmt; dann gilt der ganze Block als ungültig. */
static bool restore_settings(rof_sensor_registry_t *reg, const uint8_t *buf, size_t len)
{
    if (len < STORE_HEADER_SIZE || buf[0] != STORE_VERSION || buf[1] > ROF_SENSOR_CHANNELS_MAX ||
        len != STORE_HEADER_SIZE + (size_t)buf[1] * STORE_ENTRY_SIZE) {
        return false;
    }
    for (size_t i = 0; i < buf[1]; i++) {
        const uint8_t *entry = &buf[STORE_HEADER_SIZE + i * STORE_ENTRY_SIZE];
        const rof_sensor_id_t id = get_u64(entry);
        const rof_role_t role = (rof_role_t)entry[8];
        const float offset_c = get_f32(&entry[9]);
        if (!role_is_valid(role) || !offset_is_valid(offset_c) || find_channel(reg, id) != NULL ||
            (role != ROF_ROLE_UNASSIGNED && find_role(reg, role) != NULL)) {
            return false;
        }
        reg->channels[reg->channel_count++] = (rof_sensor_channel_t){
            .id = id,
            .type = "",
            .role = role,
            .offset_c = offset_c,
            .read_status = ROF_ERR_NOT_FOUND,
        };
    }
    return true;
}

/* Ohne gültige gespeicherte Daten startet die Verwaltung ohne Zuordnung.
 * Der Garraum ist dann nicht zugeordnet, und die Sicherheitslogik
 * verhindert das Heizen (sicherer Standard). */
static void load_settings(rof_sensor_registry_t *reg)
{
    uint8_t buf[STORE_MAX_SIZE];
    size_t len = 0;
    rof_status_t status = rof_storage_load(reg->storage, ROF_SENSOR_STORAGE_KEY, buf, sizeof(buf), &len);
    if (status == ROF_ERR_NOT_FOUND) {
        return; /* erster Start */
    }
    if (status != ROF_OK) {
        rof_log_write(reg->logger, ROF_LOG_ERROR, "Sensorzuordnung nicht lesbar, Start ohne Zuordnung");
        return;
    }
    if (!restore_settings(reg, buf, len)) {
        reg->channel_count = 0;
        rof_log_write(reg->logger, ROF_LOG_ERROR,
                      "Gespeicherte Sensorzuordnung ungültig, Start ohne Zuordnung");
    }
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
    load_settings(reg);
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
    for (size_t i = 0; i < reg->channel_count; i++) {
        read_channel(&reg->channels[i]);
    }
}

rof_status_t rof_sensor_registry_set_offset(rof_sensor_registry_t *reg, rof_sensor_id_t id,
                                            float offset_c)
{
    if (!offset_is_valid(offset_c)) {
        return ROF_ERR_INVALID_ARG;
    }
    rof_sensor_channel_t *ch = find_channel(reg, id);
    if (ch == NULL) {
        return ROF_ERR_NOT_FOUND;
    }
    const float old_offset_c = ch->offset_c;
    ch->offset_c = offset_c;
    rof_status_t status = save_settings(reg);
    if (status != ROF_OK) {
        ch->offset_c = old_offset_c;
        return status;
    }
    ch->temp_c = ch->raw_c + offset_c;
    return ROF_OK;
}

rof_status_t rof_sensor_registry_assign(rof_sensor_registry_t *reg, rof_sensor_id_t id,
                                        rof_role_t role)
{
    if (!role_is_valid(role)) {
        return ROF_ERR_INVALID_ARG;
    }
    rof_sensor_channel_t *ch = find_channel(reg, id);
    if (ch == NULL) {
        return ROF_ERR_NOT_FOUND;
    }

    /* Bisherigen Inhaber der Rolle merken, damit beides zurückgenommen werden kann. */
    rof_sensor_channel_t *previous = NULL;
    if (role != ROF_ROLE_UNASSIGNED) {
        previous = find_role(reg, role);
    }
    const rof_role_t old_role = ch->role;
    if (previous != NULL) {
        previous->role = ROF_ROLE_UNASSIGNED;
    }
    ch->role = role;

    rof_status_t status = save_settings(reg);
    if (status != ROF_OK) {
        ch->role = old_role;
        if (previous != NULL) {
            previous->role = role;
        }
        return status;
    }
    return ROF_OK;
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
