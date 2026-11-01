/*
 * Tests von proto: Anfragen dekodieren und prüfen, Zustand kodieren
 * (API-02, API-04). Kaputtes JSON, fehlende Felder und Werte außerhalb
 * der Grenzen müssen mit dem richtigen Grund abgelehnt werden.
 */
#include "unity.h"

#include "rof_proto.h"

#include <string.h>

static rof_command_t cmd;
static rof_limits_t limits;

void setUp(void)
{
    memset(&cmd, 0xAB, sizeof(cmd));
    limits = (rof_limits_t){
        .setpoint_min_c = 20.0f,
        .setpoint_max_c = 110.0f,
        .offset_max_c = 5.0f,
        .hard_max_c = 120.0f,
    };
}

void tearDown(void) {}

static rof_proto_result_t setpoint(const char *body)
{
    return rof_proto_decode_setpoint(body, strlen(body), &limits, &cmd);
}

static rof_proto_result_t role(const char *id, const char *body)
{
    return rof_proto_decode_role(id, body, strlen(body), &cmd);
}

static rof_proto_result_t offset(const char *id, const char *body)
{
    return rof_proto_decode_offset(id, body, strlen(body), &limits, &cmd);
}

/* ---- Sollwert ----------------------------------------------------------- */

/* API-02 */
static void test_setpoint_is_decoded(void)
{
    TEST_ASSERT_EQUAL(ROF_PROTO_OK, setpoint("{\"setpoint_c\": 80}").status);
    TEST_ASSERT_EQUAL(ROF_COMMAND_SET_SETPOINT, cmd.type);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 80.0f, cmd.value_c);
}

/* API-04: kaputtes JSON */
static void test_broken_json_is_bad_json(void)
{
    TEST_ASSERT_EQUAL(ROF_PROTO_BAD_JSON, setpoint("").status);
    TEST_ASSERT_EQUAL(ROF_PROTO_BAD_JSON, setpoint("{\"setpoint_c\": 80").status);
    TEST_ASSERT_EQUAL(ROF_PROTO_BAD_JSON, setpoint("{\"setpoint_c\": {\"v\": 80}}").status);
    TEST_ASSERT_EQUAL(ROF_PROTO_BAD_JSON, setpoint("{\"setpoint_c\": 80, \"setpoint_c\": 90}").status);
    TEST_ASSERT_EQUAL(ROF_PROTO_BAD_JSON, setpoint("setpoint_c=80").status);
}

/* API-04: fehlendes Feld, mit Namen */
static void test_missing_field_is_named(void)
{
    rof_proto_result_t r = setpoint("{}");

    TEST_ASSERT_EQUAL(ROF_PROTO_MISSING_FIELD, r.status);
    TEST_ASSERT_EQUAL_STRING("setpoint_c", r.field);
}

static void test_wrong_type_is_named(void)
{
    rof_proto_result_t r = setpoint("{\"setpoint_c\": \"80\"}");
    TEST_ASSERT_EQUAL(ROF_PROTO_WRONG_TYPE, r.status);
    TEST_ASSERT_EQUAL_STRING("setpoint_c", r.field);

    TEST_ASSERT_EQUAL(ROF_PROTO_WRONG_TYPE, setpoint("{\"setpoint_c\": null}").status);
}

/* E-030: unbekannte Felder werden abgelehnt, mit Namen */
static void test_unknown_field_is_named(void)
{
    rof_proto_result_t r = setpoint("{\"setpoint_c\": 80, \"foo\": 1}");

    TEST_ASSERT_EQUAL(ROF_PROTO_UNKNOWN_FIELD, r.status);
    TEST_ASSERT_EQUAL_STRING("foo", r.field);
}

/* API-04, SAF-09: außerhalb der Grenzen abgelehnt mit demselben Grund wie im Regeltask */
static void test_setpoint_outside_limits_is_rejected_with_reason(void)
{
    rof_proto_result_t r = setpoint("{\"setpoint_c\": 19.9}");
    TEST_ASSERT_EQUAL(ROF_PROTO_OUT_OF_RANGE, r.status);
    TEST_ASSERT_EQUAL(ROF_CMD_SETPOINT_TOO_LOW, r.result);

    r = setpoint("{\"setpoint_c\": 110.1}");
    TEST_ASSERT_EQUAL(ROF_CMD_SETPOINT_TOO_HIGH, r.result);

    r = setpoint("{\"setpoint_c\": 1e999}"); /* gültiges JSON, aber unendlich als float */
    TEST_ASSERT_EQUAL(ROF_PROTO_OUT_OF_RANGE, r.status);
    TEST_ASSERT_EQUAL(ROF_CMD_SETPOINT_NOT_A_NUMBER, r.result);
}

/* Die Grenzen selbst sind erlaubt */
static void test_setpoint_at_limits_is_accepted(void)
{
    TEST_ASSERT_EQUAL(ROF_PROTO_OK, setpoint("{\"setpoint_c\": 20}").status);
    TEST_ASSERT_EQUAL(ROF_PROTO_OK, setpoint("{\"setpoint_c\": 110}").status);
}

/* Anfragen über 256 Byte werden gar nicht erst gelesen */
static void test_too_large_body(void)
{
    char body[ROF_PROTO_MAX_BODY + 2];
    memset(body, ' ', sizeof(body) - 1);
    body[0] = '{';
    body[sizeof(body) - 2] = '}';
    body[sizeof(body) - 1] = '\0';

    TEST_ASSERT_EQUAL(ROF_PROTO_TOO_LARGE, setpoint(body).status);
}

/* ---- Befehle ohne Inhalt ------------------------------------------------ */

static void test_start_stop_reset_accept_empty_body_or_empty_object(void)
{
    TEST_ASSERT_EQUAL(ROF_PROTO_OK, rof_proto_decode_empty(ROF_COMMAND_START, "", 0, &cmd).status);
    TEST_ASSERT_EQUAL(ROF_COMMAND_START, cmd.type);
    TEST_ASSERT_EQUAL(ROF_PROTO_OK, rof_proto_decode_empty(ROF_COMMAND_STOP, "{ }", 3, &cmd).status);
    TEST_ASSERT_EQUAL(ROF_COMMAND_STOP, cmd.type);
    TEST_ASSERT_EQUAL(ROF_PROTO_OK, rof_proto_decode_empty(ROF_COMMAND_RESET, "{}", 2, &cmd).status);
    TEST_ASSERT_EQUAL(ROF_COMMAND_RESET, cmd.type);
}

static void test_empty_command_rejects_fields_and_broken_json(void)
{
    rof_proto_result_t r = rof_proto_decode_empty(ROF_COMMAND_START, "{\"now\":true}", 12, &cmd);
    TEST_ASSERT_EQUAL(ROF_PROTO_UNKNOWN_FIELD, r.status);
    TEST_ASSERT_EQUAL_STRING("now", r.field);

    TEST_ASSERT_EQUAL(ROF_PROTO_BAD_JSON, rof_proto_decode_empty(ROF_COMMAND_START, "go", 2, &cmd).status);
}

/* ---- Rolle und Offset --------------------------------------------------- */

/* API-02, SEN-02 */
static void test_role_is_decoded(void)
{
    TEST_ASSERT_EQUAL(ROF_PROTO_OK, role("28AA000000000001", "{\"role\":\"chamber\"}").status);
    TEST_ASSERT_EQUAL(ROF_COMMAND_ASSIGN_ROLE, cmd.type);
    TEST_ASSERT_EQUAL_UINT64(0x28AA000000000001ULL, cmd.sensor_id);
    TEST_ASSERT_EQUAL(ROF_ROLE_CHAMBER, cmd.role);

    TEST_ASSERT_EQUAL(ROF_PROTO_OK, role("28AA000000000001", "{\"role\":\"unassigned\"}").status);
    TEST_ASSERT_EQUAL(ROF_ROLE_UNASSIGNED, cmd.role);
}

static void test_unknown_role_name_is_rejected(void)
{
    rof_proto_result_t r = role("28AA000000000001", "{\"role\":\"oven\"}");

    TEST_ASSERT_EQUAL(ROF_PROTO_UNKNOWN_VALUE, r.status);
    TEST_ASSERT_EQUAL_STRING("role", r.field);
    TEST_ASSERT_EQUAL(ROF_PROTO_WRONG_TYPE, role("28AA000000000001", "{\"role\":1}").status);
    TEST_ASSERT_EQUAL(ROF_PROTO_MISSING_FIELD, role("28AA000000000001", "{}").status);
}

/* E-028: IDs sind genau 16 Hex-Ziffern in Großbuchstaben */
static void test_bad_sensor_id_is_rejected(void)
{
    const char *body = "{\"role\":\"chamber\"}";
    TEST_ASSERT_EQUAL(ROF_PROTO_BAD_SENSOR_ID, role("28aa000000000001", body).status);
    TEST_ASSERT_EQUAL(ROF_PROTO_BAD_SENSOR_ID, role("28AA00000000001", body).status);
    TEST_ASSERT_EQUAL(ROF_PROTO_BAD_SENSOR_ID, role("28AA0000000000011", body).status);
    TEST_ASSERT_EQUAL(ROF_PROTO_BAD_SENSOR_ID, role("28AG000000000001", body).status);
    TEST_ASSERT_EQUAL(ROF_PROTO_BAD_SENSOR_ID, role("", body).status);
}

static void test_sensor_id_round_trip(void)
{
    rof_sensor_id_t id = 0;
    char text[ROF_PROTO_SENSOR_ID_LEN + 1];

    TEST_ASSERT_TRUE(rof_proto_parse_sensor_id("FFFFFFFFFFFFFFFF", &id));
    TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, id);
    rof_proto_format_sensor_id(0x28BB000000000002ULL, text);
    TEST_ASSERT_EQUAL_STRING("28BB000000000002", text);
}

/* API-02, SEN-01 */
static void test_offset_is_decoded(void)
{
    TEST_ASSERT_EQUAL(ROF_PROTO_OK, offset("28BB000000000002", "{\"offset_c\":-0.5}").status);
    TEST_ASSERT_EQUAL(ROF_COMMAND_SET_OFFSET, cmd.type);
    TEST_ASSERT_EQUAL_UINT64(0x28BB000000000002ULL, cmd.sensor_id);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.5f, cmd.value_c);
}

/* API-04: Offset außerhalb ±5 °C, Grenzen selbst erlaubt */
static void test_offset_outside_limits_is_rejected(void)
{
    rof_proto_result_t r = offset("28BB000000000002", "{\"offset_c\":5.5}");
    TEST_ASSERT_EQUAL(ROF_PROTO_OUT_OF_RANGE, r.status);
    TEST_ASSERT_EQUAL(ROF_CMD_INVALID_VALUE, r.result);
    TEST_ASSERT_EQUAL(ROF_PROTO_OUT_OF_RANGE, offset("28BB000000000002", "{\"offset_c\":-1e999}").status);

    TEST_ASSERT_EQUAL(ROF_PROTO_OK, offset("28BB000000000002", "{\"offset_c\":5}").status);
    TEST_ASSERT_EQUAL(ROF_PROTO_OK, offset("28BB000000000002", "{\"offset_c\":-5}").status);
}

/* ---- Namen (E-028): genau die Listen aus docs/api.yaml ------------------ */

static void test_names_match_api_description(void)
{
    const char *states[] = { "idle", "heating", "holding", "cooling", "fault", "safe_state" };
    for (int i = 0; i < 6; i++) {
        TEST_ASSERT_EQUAL_STRING(states[i], rof_proto_state_name((rof_state_t)i));
    }
    const char *roles[] = { "unassigned", "chamber", "probe_1", "probe_2", "smoke_gen" };
    for (int i = 0; i < ROF_ROLE_COUNT; i++) {
        TEST_ASSERT_EQUAL_STRING(roles[i], rof_proto_role_name((rof_role_t)i));
    }
    const char *role_states[] = { "ok", "unassigned", "missing", "read_error", "duplicate" };
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_STRING(role_states[i], rof_proto_role_status_name((rof_role_status_t)i));
    }
    const char *faults[] = { "none", "chamber_unassigned", "chamber_missing", "chamber_read_error",
                             "chamber_duplicate", "implausible", "over_temperature", "frozen",
                             "cycle_overrun" };
    for (int i = 0; i < 9; i++) {
        TEST_ASSERT_EQUAL_STRING(faults[i], rof_proto_fault_name((rof_fault_t)i));
    }
    const char *commands[] = { "set_setpoint", "start", "stop", "assign_role", "set_offset", "reset" };
    for (int i = 0; i < 6; i++) {
        TEST_ASSERT_EQUAL_STRING(commands[i], rof_proto_command_name((rof_command_type_t)(i + 1)));
    }
    const char *results[] = { "ok", "no_setpoint", "already_running", "fault_active", "safe_state",
                              "setpoint_too_low", "setpoint_too_high", "setpoint_not_a_number",
                              "sensors_locked", "unknown_sensor", "invalid_value", "storage_error",
                              "not_in_safe_state", "cause_active", "unknown_command" };
    for (int i = 0; i < 15; i++) {
        TEST_ASSERT_EQUAL_STRING(results[i], rof_proto_result_name((rof_cmd_result_t)i));
    }
    TEST_ASSERT_EQUAL_STRING("ok", rof_proto_read_status_name(ROF_OK));
    TEST_ASSERT_EQUAL_STRING("not_found", rof_proto_read_status_name(ROF_ERR_NOT_FOUND));
    TEST_ASSERT_EQUAL_STRING("read_error", rof_proto_read_status_name(ROF_ERR_IO));
}

/* Ein Wert außerhalb der Liste ist ein Fehler im Programm; er wird nie zu
 * einem gültigen Namen, sondern zu "unknown". */
static void test_values_outside_lists_are_unknown(void)
{
    TEST_ASSERT_EQUAL_STRING("unknown", rof_proto_state_name((rof_state_t)99));
    TEST_ASSERT_EQUAL_STRING("unknown", rof_proto_command_name((rof_command_type_t)0));
    TEST_ASSERT_EQUAL_STRING("unknown", rof_proto_result_name((rof_cmd_result_t)99));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_setpoint_is_decoded);
    RUN_TEST(test_broken_json_is_bad_json);
    RUN_TEST(test_missing_field_is_named);
    RUN_TEST(test_wrong_type_is_named);
    RUN_TEST(test_unknown_field_is_named);
    RUN_TEST(test_setpoint_outside_limits_is_rejected_with_reason);
    RUN_TEST(test_setpoint_at_limits_is_accepted);
    RUN_TEST(test_too_large_body);

    RUN_TEST(test_start_stop_reset_accept_empty_body_or_empty_object);
    RUN_TEST(test_empty_command_rejects_fields_and_broken_json);

    RUN_TEST(test_role_is_decoded);
    RUN_TEST(test_unknown_role_name_is_rejected);
    RUN_TEST(test_bad_sensor_id_is_rejected);
    RUN_TEST(test_sensor_id_round_trip);
    RUN_TEST(test_offset_is_decoded);
    RUN_TEST(test_offset_outside_limits_is_rejected);

    RUN_TEST(test_names_match_api_description);
    RUN_TEST(test_values_outside_lists_are_unknown);

    return UNITY_END();
}
