#include "rof_proto.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

/* ---- Helfer ------------------------------------------------------------- */

static rof_proto_result_t result_of(rof_proto_status_t status, const char *field)
{
    rof_proto_result_t r = { .status = status, .field = "", .result = ROF_CMD_OK };
    if (field != NULL) {
        snprintf(r.field, sizeof(r.field), "%s", field);
    }
    return r;
}

/* Wert außerhalb der Grenzen, mit demselben Grund wie im Regeltask */
static rof_proto_result_t out_of_range(const char *field, rof_cmd_result_t reason)
{
    rof_proto_result_t r = result_of(ROF_PROTO_OUT_OF_RANGE, field);
    r.result = reason;
    return r;
}

/* So formuliert, dass NaN und unendlich keine endlichen Zahlen sind */
static bool is_finite(float value)
{
    return value >= -FLT_MAX && value <= FLT_MAX;
}

/* Liest den Inhalt und lehnt jedes Feld ab, das nicht in allowed steht (E-030). */
static rof_proto_result_t read_body(const char *body, size_t len, const char *const *allowed,
                                    size_t allowed_count, rof_json_object_t *obj)
{
    if (len > ROF_PROTO_MAX_BODY) {
        return result_of(ROF_PROTO_TOO_LARGE, NULL);
    }
    if (rof_json_parse_flat(body, len, obj) != ROF_JSON_OK) {
        return result_of(ROF_PROTO_BAD_JSON, NULL);
    }
    for (size_t i = 0; i < obj->count; i++) {
        bool known = false;
        for (size_t k = 0; k < allowed_count; k++) {
            known = known || strcmp(obj->fields[i].key, allowed[k]) == 0;
        }
        if (!known) {
            return result_of(ROF_PROTO_UNKNOWN_FIELD, obj->fields[i].key);
        }
    }
    return result_of(ROF_PROTO_OK, NULL);
}

static rof_proto_result_t require_field(const rof_json_object_t *obj, const char *key,
                                        rof_json_type_t type, const rof_json_field_t **field)
{
    *field = rof_json_find(obj, key);
    if (*field == NULL) {
        return result_of(ROF_PROTO_MISSING_FIELD, key);
    }
    if ((*field)->type != type) {
        return result_of(ROF_PROTO_WRONG_TYPE, key);
    }
    return result_of(ROF_PROTO_OK, NULL);
}

static int hex_digit(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1; /* auch Kleinbuchstaben: Die API verlangt Großbuchstaben */
}

/* ---- Sensor-IDs --------------------------------------------------------- */

bool rof_proto_parse_sensor_id(const char *text, rof_sensor_id_t *id)
{
    if (strlen(text) != ROF_PROTO_SENSOR_ID_LEN) {
        return false;
    }
    rof_sensor_id_t value = 0;
    for (size_t i = 0; i < ROF_PROTO_SENSOR_ID_LEN; i++) {
        const int digit = hex_digit(text[i]);
        if (digit < 0) {
            return false;
        }
        value = (value << 4) | (rof_sensor_id_t)digit;
    }
    *id = value;
    return true;
}

void rof_proto_format_sensor_id(rof_sensor_id_t id, char out[ROF_PROTO_SENSOR_ID_LEN + 1])
{
    snprintf(out, ROF_PROTO_SENSOR_ID_LEN + 1, "%016llX", (unsigned long long)id);
}

/* ---- Anfragen ----------------------------------------------------------- */

rof_proto_result_t rof_proto_decode_setpoint(const char *body, size_t len, const rof_limits_t *limits,
                                             rof_command_t *cmd)
{
    static const char *const allowed[] = { "setpoint_c" };
    rof_json_object_t obj;
    rof_proto_result_t r = read_body(body, len, allowed, 1, &obj);
    if (r.status != ROF_PROTO_OK) {
        return r;
    }
    const rof_json_field_t *field;
    r = require_field(&obj, "setpoint_c", ROF_JSON_NUMBER, &field);
    if (r.status != ROF_PROTO_OK) {
        return r;
    }
    const float setpoint_c = field->number;
    if (!is_finite(setpoint_c)) {
        return out_of_range("setpoint_c", ROF_CMD_SETPOINT_NOT_A_NUMBER);
    }
    if (setpoint_c < limits->setpoint_min_c) {
        return out_of_range("setpoint_c", ROF_CMD_SETPOINT_TOO_LOW);
    }
    if (setpoint_c > limits->setpoint_max_c) {
        return out_of_range("setpoint_c", ROF_CMD_SETPOINT_TOO_HIGH);
    }
    *cmd = (rof_command_t){ .type = ROF_COMMAND_SET_SETPOINT, .value_c = setpoint_c };
    return r;
}

rof_proto_result_t rof_proto_decode_core_target(const char *body, size_t len, const rof_limits_t *limits,
                                                rof_command_t *cmd)
{
    static const char *const allowed[] = { "core_target_c" };
    rof_json_object_t obj;
    rof_proto_result_t r = read_body(body, len, allowed, 1, &obj);
    if (r.status != ROF_PROTO_OK) {
        return r;
    }
    const rof_json_field_t *field;
    r = require_field(&obj, "core_target_c", ROF_JSON_NUMBER, &field);
    if (r.status != ROF_PROTO_OK) {
        return r;
    }
    const float target_c = field->number;
    if (!is_finite(target_c)) {
        return out_of_range("core_target_c", ROF_CMD_INVALID_VALUE);
    }
    if (target_c < limits->core_target_min_c) {
        return out_of_range("core_target_c", ROF_CMD_CORE_TARGET_TOO_LOW);
    }
    if (target_c > limits->core_target_max_c) {
        return out_of_range("core_target_c", ROF_CMD_CORE_TARGET_TOO_HIGH);
    }
    *cmd = (rof_command_t){ .type = ROF_COMMAND_SET_CORE_TARGET, .value_c = target_c };
    return r;
}

rof_proto_result_t rof_proto_decode_empty(rof_command_type_t type, const char *body, size_t len,
                                          rof_command_t *cmd)
{
    rof_proto_result_t r = result_of(ROF_PROTO_OK, NULL);
    if (len > 0) {
        rof_json_object_t obj;
        r = read_body(body, len, NULL, 0, &obj); /* nur {} ist erlaubt */
        if (r.status != ROF_PROTO_OK) {
            return r;
        }
    }
    *cmd = (rof_command_t){ .type = type };
    return r;
}

rof_proto_result_t rof_proto_decode_role(const char *sensor_id, const char *body, size_t len,
                                         rof_command_t *cmd)
{
    rof_sensor_id_t id;
    if (!rof_proto_parse_sensor_id(sensor_id, &id)) {
        return result_of(ROF_PROTO_BAD_SENSOR_ID, "id");
    }
    static const char *const allowed[] = { "role" };
    rof_json_object_t obj;
    rof_proto_result_t r = read_body(body, len, allowed, 1, &obj);
    if (r.status != ROF_PROTO_OK) {
        return r;
    }
    const rof_json_field_t *field;
    r = require_field(&obj, "role", ROF_JSON_STRING, &field);
    if (r.status != ROF_PROTO_OK) {
        return r;
    }
    rof_role_t role;
    if (!rof_proto_role_from_name(field->string, &role)) {
        return result_of(ROF_PROTO_UNKNOWN_VALUE, "role");
    }
    *cmd = (rof_command_t){ .type = ROF_COMMAND_ASSIGN_ROLE, .sensor_id = id, .role = role };
    return r;
}

rof_proto_result_t rof_proto_decode_offset(const char *sensor_id, const char *body, size_t len,
                                           const rof_limits_t *limits, rof_command_t *cmd)
{
    rof_sensor_id_t id;
    if (!rof_proto_parse_sensor_id(sensor_id, &id)) {
        return result_of(ROF_PROTO_BAD_SENSOR_ID, "id");
    }
    static const char *const allowed[] = { "offset_c" };
    rof_json_object_t obj;
    rof_proto_result_t r = read_body(body, len, allowed, 1, &obj);
    if (r.status != ROF_PROTO_OK) {
        return r;
    }
    const rof_json_field_t *field;
    r = require_field(&obj, "offset_c", ROF_JSON_NUMBER, &field);
    if (r.status != ROF_PROTO_OK) {
        return r;
    }
    const float offset_c = field->number;
    if (!(offset_c >= -limits->offset_max_c && offset_c <= limits->offset_max_c)) {
        return out_of_range("offset_c", ROF_CMD_INVALID_VALUE);
    }
    *cmd = (rof_command_t){ .type = ROF_COMMAND_SET_OFFSET, .sensor_id = id, .value_c = offset_c };
    return r;
}
