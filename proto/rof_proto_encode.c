#include "rof_proto.h"

#include "rof_json_writer.h"

/* ---- Helfer ------------------------------------------------------------- */

static bool finish(const rof_json_writer_t *w, size_t *len)
{
    *len = w->len;
    return rof_json_writer_ok(w);
}

/* Temperatur nur, wenn sie gültig ist, sonst null (E-028) */
static void write_temperature(rof_json_writer_t *w, bool valid, float temp_c)
{
    if (valid) {
        rof_json_number(w, temp_c, 2);
    } else {
        rof_json_null(w);
    }
}

static void write_role(rof_json_writer_t *w, rof_role_t role, const rof_role_snapshot_t *reading)
{
    rof_json_key(w, rof_proto_role_name(role));
    rof_json_object_begin(w);
    rof_json_key(w, "status");
    rof_json_string(w, rof_proto_role_status_name(reading->status));
    rof_json_key(w, "temp_c");
    write_temperature(w, reading->status == ROF_ROLE_STATUS_OK, reading->temp_c);
    rof_json_object_end(w);
}

static void write_channel(rof_json_writer_t *w, const rof_channel_snapshot_t *ch)
{
    char id[ROF_PROTO_SENSOR_ID_LEN + 1];
    rof_proto_format_sensor_id(ch->id, id);
    rof_json_object_begin(w);
    rof_json_key(w, "id");
    rof_json_string(w, id);
    rof_json_key(w, "type");
    rof_json_string(w, ch->type != NULL ? ch->type : "");
    rof_json_key(w, "role");
    rof_json_string(w, rof_proto_role_name(ch->role));
    rof_json_key(w, "offset_c");
    rof_json_number(w, ch->offset_c, 2);
    rof_json_key(w, "present");
    rof_json_bool(w, ch->present);
    rof_json_key(w, "duplicate");
    rof_json_bool(w, ch->duplicate);
    rof_json_key(w, "read_status");
    rof_json_string(w, rof_proto_read_status_name(ch->read_status));
    rof_json_key(w, "temp_c");
    write_temperature(w, ch->present && !ch->duplicate && ch->read_status == ROF_OK, ch->temp_c);
    rof_json_object_end(w);
}

static void write_command_record(rof_json_writer_t *w, const rof_command_record_t *record)
{
    rof_json_object_begin(w);
    rof_json_key(w, "seq");
    rof_json_uint(w, record->seq);
    rof_json_key(w, "command");
    rof_json_string(w, rof_proto_command_name(record->type));
    rof_json_key(w, "result");
    rof_json_string(w, rof_proto_result_name(record->result));
    rof_json_object_end(w);
}

static const char *error_name(rof_proto_status_t status)
{
    switch (status) {
    case ROF_PROTO_BAD_JSON:
        return "bad_json";
    case ROF_PROTO_MISSING_FIELD:
        return "missing_field";
    case ROF_PROTO_WRONG_TYPE:
        return "wrong_type";
    case ROF_PROTO_UNKNOWN_FIELD:
        return "unknown_field";
    case ROF_PROTO_UNKNOWN_VALUE:
        return "unknown_value";
    case ROF_PROTO_BAD_SENSOR_ID:
        return "bad_sensor_id";
    case ROF_PROTO_TOO_LARGE:
        return "too_large";
    default:
        return "unknown";
    }
}

/* ---- Antworten ---------------------------------------------------------- */

bool rof_proto_encode_state(const rof_snapshot_t *snapshot, char *buf, size_t size, size_t *len)
{
    const rof_snapshot_t *s = snapshot;
    rof_json_writer_t w;
    rof_json_writer_init(&w, buf, size);
    rof_json_object_begin(&w);

    rof_json_key(&w, "time_ms");
    rof_json_uint(&w, s->time_ms);
    rof_json_key(&w, "state");
    rof_json_string(&w, rof_proto_state_name(s->state));
    rof_json_key(&w, "setpoint_c");
    write_temperature(&w, s->has_setpoint, s->setpoint_c);
    rof_json_key(&w, "duty");
    rof_json_number(&w, s->duty, 2);
    rof_json_key(&w, "latched_fault");
    rof_json_string(&w, rof_proto_fault_name(s->latched_fault));
    rof_json_key(&w, "active_fault");
    rof_json_string(&w, rof_proto_fault_name(s->active_fault));

    rof_json_key(&w, "roles");
    rof_json_object_begin(&w);
    for (int role = ROF_ROLE_UNASSIGNED + 1; role < ROF_ROLE_COUNT; role++) {
        write_role(&w, (rof_role_t)role, &s->roles[role]);
    }
    rof_json_object_end(&w);

    rof_json_key(&w, "channels");
    rof_json_array_begin(&w);
    for (size_t i = 0; i < s->channel_count && i < ROF_SENSOR_CHANNELS_MAX; i++) {
        write_channel(&w, &s->channels[i]);
    }
    rof_json_array_end(&w);

    rof_json_key(&w, "sensor_overflow");
    rof_json_bool(&w, s->sensor_overflow);

    rof_json_key(&w, "recent_commands");
    rof_json_array_begin(&w);
    for (size_t i = 0; i < s->recent_count && i < ROF_CONTROLLER_RECENT_RESULTS; i++) {
        write_command_record(&w, &s->recent[i]);
    }
    rof_json_array_end(&w);

    rof_json_object_end(&w);
    return finish(&w, len);
}

bool rof_proto_encode_limits(const rof_limits_t *limits, char *buf, size_t size, size_t *len)
{
    rof_json_writer_t w;
    rof_json_writer_init(&w, buf, size);
    rof_json_object_begin(&w);
    rof_json_key(&w, "setpoint_min_c");
    rof_json_number(&w, limits->setpoint_min_c, 2);
    rof_json_key(&w, "setpoint_max_c");
    rof_json_number(&w, limits->setpoint_max_c, 2);
    rof_json_key(&w, "offset_max_c");
    rof_json_number(&w, limits->offset_max_c, 2);
    rof_json_key(&w, "hard_max_c");
    rof_json_number(&w, limits->hard_max_c, 2);
    rof_json_object_end(&w);
    return finish(&w, len);
}

bool rof_proto_encode_command_response(uint32_t seq, rof_cmd_result_t result, char *buf, size_t size,
                                       size_t *len)
{
    rof_json_writer_t w;
    rof_json_writer_init(&w, buf, size);
    rof_json_object_begin(&w);
    rof_json_key(&w, "seq");
    rof_json_uint(&w, seq);
    rof_json_key(&w, "result");
    rof_json_string(&w, rof_proto_result_name(result));
    rof_json_object_end(&w);
    return finish(&w, len);
}

bool rof_proto_encode_rejection(const rof_proto_result_t *rejection, char *buf, size_t size, size_t *len)
{
    rof_json_writer_t w;
    rof_json_writer_init(&w, buf, size);
    rof_json_object_begin(&w);
    if (rejection->status == ROF_PROTO_OUT_OF_RANGE) {
        /* derselbe Aufbau wie eine Ablehnung durch den Regeltask, nur ohne seq */
        rof_json_key(&w, "result");
        rof_json_string(&w, rof_proto_result_name(rejection->result));
    } else {
        rof_json_key(&w, "error");
        rof_json_string(&w, error_name(rejection->status));
        if (rejection->field[0] != '\0') {
            rof_json_key(&w, "field");
            rof_json_string(&w, rejection->field);
        }
    }
    rof_json_object_end(&w);
    return finish(&w, len);
}

bool rof_proto_encode_error(const char *error, bool has_seq, uint32_t seq, char *buf, size_t size,
                            size_t *len)
{
    rof_json_writer_t w;
    rof_json_writer_init(&w, buf, size);
    rof_json_object_begin(&w);
    rof_json_key(&w, "error");
    rof_json_string(&w, error);
    if (has_seq) {
        rof_json_key(&w, "seq");
        rof_json_uint(&w, seq);
    }
    rof_json_object_end(&w);
    return finish(&w, len);
}

/* ---- HTTP-Status (E-028) ------------------------------------------------ */

int rof_proto_http_status(rof_cmd_result_t result)
{
    switch (result) {
    case ROF_CMD_OK:
        return 200;
    case ROF_CMD_NO_SETPOINT:
    case ROF_CMD_ALREADY_RUNNING:
    case ROF_CMD_FAULT_ACTIVE:
    case ROF_CMD_SAFE_STATE:
    case ROF_CMD_SENSORS_LOCKED:
    case ROF_CMD_NOT_IN_SAFE_STATE:
    case ROF_CMD_CAUSE_ACTIVE:
        return 409; /* passt nicht zum Zustand */
    case ROF_CMD_SETPOINT_TOO_LOW:
    case ROF_CMD_SETPOINT_TOO_HIGH:
    case ROF_CMD_SETPOINT_NOT_A_NUMBER:
    case ROF_CMD_UNKNOWN_SENSOR:
    case ROF_CMD_INVALID_VALUE:
        return 422; /* Wert außerhalb der Grenzen oder unbekannt */
    case ROF_CMD_STORAGE_ERROR:
        return 500;
    default:
        return 400; /* unbekannter Befehl, kommt vom eigenen Webserver nicht vor */
    }
}

int rof_proto_rejection_http_status(const rof_proto_result_t *rejection)
{
    if (rejection->status == ROF_PROTO_TOO_LARGE) {
        return 413;
    }
    if (rejection->status == ROF_PROTO_OUT_OF_RANGE) {
        return 422;
    }
    return 400;
}
