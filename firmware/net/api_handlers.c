#include "api_handlers.h"

#include "request_path.h"
#include "rof_proto.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>

static const char *const TAG = "api";

typedef enum { API_SETPOINT, API_CORE_TARGET, API_START, API_STOP, API_RESET, API_SENSOR } api_command_t;
static const api_command_t KIND_SETPOINT = API_SETPOINT;
static const api_command_t KIND_CORE_TARGET = API_CORE_TARGET;
static const api_command_t KIND_START = API_START;
static const api_command_t KIND_STOP = API_STOP;
static const api_command_t KIND_RESET = API_RESET;
static const api_command_t KIND_SENSOR = API_SENSOR;

static command_queue_t *api_queue;
static snapshot_store_t *api_store;
/* Alles Folgende benutzt nur der eine Task des HTTP-Servers */
static uint32_t next_seq = 1;
static rof_snapshot_t snapshot;
static char body[ROF_PROTO_MAX_BODY];
static char json[ROF_PROTO_STATE_JSON_MAX];

/* ---- Antworten ----------------------------------------------------------- */

static const char *status_line(int status)
{
    switch (status) {
    case 200:
        return "200 OK";
    case 400:
        return "400 Bad Request";
    case 409:
        return "409 Conflict";
    case 413:
        return "413 Content Too Large";
    case 422:
        return "422 Unprocessable Content";
    case 503:
        return "503 Service Unavailable";
    case 504:
        return "504 Gateway Timeout";
    default:
        return "500 Internal Server Error";
    }
}

static esp_err_t send_json(httpd_req_t *req, int status, size_t len)
{
    httpd_resp_set_status(req, status_line(status));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json, (ssize_t)len);
}

/* Fehler des Webservers selbst: busy, timeout, not_ready */
static esp_err_t send_error(httpd_req_t *req, int status, const char *error, bool has_seq, uint32_t seq)
{
    size_t len = 0;
    if (!rof_proto_encode_error(error, has_seq, seq, json, sizeof(json), &len)) {
        return httpd_resp_send_500(req);
    }
    return send_json(req, status, len);
}

static esp_err_t send_rejection(httpd_req_t *req, const rof_proto_result_t *rejection)
{
    size_t len = 0;
    if (!rof_proto_encode_rejection(rejection, json, sizeof(json), &len)) {
        return httpd_resp_send_500(req);
    }
    return send_json(req, rof_proto_rejection_http_status(rejection), len);
}

/* Kopie des Snapshots holen. false: Es gibt noch keinen, kurz nach dem Start. */
static bool read_snapshot(void)
{
    return snapshot_store_read(api_store, &snapshot, pdMS_TO_TICKS(API_SNAPSHOT_WAIT_MS));
}

/* ---- Abfragen ------------------------------------------------------------ */

static esp_err_t state_handler(httpd_req_t *req)
{
    if (!read_snapshot()) {
        return send_error(req, 503, "not_ready", false, 0);
    }
    size_t len = 0;
    if (!rof_proto_encode_state(&snapshot, json, sizeof(json), &len)) {
        return httpd_resp_send_500(req);
    }
    return send_json(req, 200, len);
}

static esp_err_t config_handler(httpd_req_t *req)
{
    if (!read_snapshot()) {
        return send_error(req, 503, "not_ready", false, 0);
    }
    size_t len = 0;
    if (!rof_proto_encode_limits(&snapshot.limits, json, sizeof(json), &len)) {
        return httpd_resp_send_500(req);
    }
    return send_json(req, 200, len);
}

/* ---- Befehle ------------------------------------------------------------- */

/* Liest den ganzen Inhalt nach body. false, wenn die Verbindung abbricht. */
static bool read_body(httpd_req_t *req, size_t len)
{
    size_t received = 0;
    while (received < len) {
        const int n = httpd_req_recv(req, body + received, len - received);
        if (n <= 0) {
            return false;
        }
        received += (size_t)n;
    }
    return true;
}

/* Prüft die Anfrage mit proto. Bei ROF_PROTO_OK steht der Befehl in *cmd. */
static rof_proto_result_t decode(httpd_req_t *req, api_command_t kind, size_t len, rof_command_t *cmd)
{
    switch (kind) {
    case API_SETPOINT:
        return rof_proto_decode_setpoint(body, len, &snapshot.limits, cmd);
    case API_CORE_TARGET:
        return rof_proto_decode_core_target(body, len, &snapshot.limits, cmd);
    case API_START:
        return rof_proto_decode_empty(ROF_COMMAND_START, body, len, cmd);
    case API_STOP:
        return rof_proto_decode_empty(ROF_COMMAND_STOP, body, len, cmd);
    case API_RESET:
        return rof_proto_decode_empty(ROF_COMMAND_RESET, body, len, cmd);
    default: { /* Sensorpfad; NET_SENSOR_PATH_NO_MATCH hat command_handler schon abgefangen */
        char id[ROF_PROTO_SENSOR_ID_LEN + 1];
        const net_sensor_path_t path = net_parse_sensor_path(req->uri, id);
        return path == NET_SENSOR_PATH_ROLE ? rof_proto_decode_role(id, body, len, cmd)
                                            : rof_proto_decode_offset(id, body, len, &snapshot.limits, cmd);
    }
    }
}

/* Wartet im Snapshot auf das Ergebnis von seq, höchstens API_RESULT_TIMEOUT_MS */
static bool wait_for_result(uint32_t seq, rof_cmd_result_t *result)
{
    const int64_t deadline_us = esp_timer_get_time() + (int64_t)API_RESULT_TIMEOUT_MS * 1000;
    while (esp_timer_get_time() < deadline_us) {
        vTaskDelay(pdMS_TO_TICKS(API_RESULT_POLL_MS));
        if (read_snapshot() && rof_proto_command_result(&snapshot, seq, result)) {
            return true;
        }
    }
    return false;
}

static esp_err_t command_handler(httpd_req_t *req)
{
    const api_command_t kind = *(const api_command_t *)req->user_ctx;
    if (kind == API_SENSOR) {
        char id[ROF_PROTO_SENSOR_ID_LEN + 1];
        if (net_parse_sensor_path(req->uri, id) == NET_SENSOR_PATH_NO_MATCH) {
            return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unbekannter Sensorpfad");
        }
    }
    /* Ohne Snapshot keine Grenzen zum Prüfen und kein Ergebnis zum Abwarten */
    if (!read_snapshot()) {
        return send_error(req, 503, "not_ready", false, 0);
    }
    /* Zu große Anfragen werden gar nicht erst gelesen. Der Rest liegt dann
     * noch auf der Verbindung, deshalb wird sie danach geschlossen. */
    if (req->content_len > ROF_PROTO_MAX_BODY) {
        const rof_proto_result_t too_large = { .status = ROF_PROTO_TOO_LARGE };
        httpd_resp_set_hdr(req, "Connection", "close");
        return send_rejection(req, &too_large);
    }
    const size_t len = req->content_len;
    if (!read_body(req, len)) {
        return ESP_FAIL; /* Verbindung abgebrochen, der Server schließt sie */
    }

    rof_command_t cmd;
    const rof_proto_result_t decoded = decode(req, kind, len, &cmd);
    if (decoded.status != ROF_PROTO_OK) {
        return send_rejection(req, &decoded);
    }
    cmd.seq = next_seq++;
    if (!command_queue_send(api_queue, &cmd)) {
        ESP_LOGW(TAG, "Befehlsqueue voll, Befehl %lu abgelehnt", (unsigned long)cmd.seq);
        return send_error(req, 503, "busy", false, 0);
    }
    rof_cmd_result_t result;
    if (!wait_for_result(cmd.seq, &result)) {
        return send_error(req, 504, "timeout", true, cmd.seq);
    }
    size_t out_len = 0;
    if (!rof_proto_encode_command_response(cmd.seq, result, json, sizeof(json), &out_len)) {
        return httpd_resp_send_500(req);
    }
    return send_json(req, rof_proto_http_status(result), out_len);
}

static const httpd_uri_t ROUTES[] = {
    { .uri = "/api/state", .method = HTTP_GET, .handler = state_handler },
    { .uri = "/api/config", .method = HTTP_GET, .handler = config_handler },
    { .uri = "/api/setpoint", .method = HTTP_POST, .handler = command_handler, .user_ctx = (void *)&KIND_SETPOINT },
    { .uri = "/api/core-target", .method = HTTP_POST, .handler = command_handler, .user_ctx = (void *)&KIND_CORE_TARGET },
    { .uri = "/api/start", .method = HTTP_POST, .handler = command_handler, .user_ctx = (void *)&KIND_START },
    { .uri = "/api/stop", .method = HTTP_POST, .handler = command_handler, .user_ctx = (void *)&KIND_STOP },
    { .uri = "/api/reset", .method = HTTP_POST, .handler = command_handler, .user_ctx = (void *)&KIND_RESET },
    { .uri = "/api/sensors/*", .method = HTTP_PUT, .handler = command_handler, .user_ctx = (void *)&KIND_SENSOR },
};
_Static_assert(sizeof(ROUTES) / sizeof(ROUTES[0]) == API_ROUTE_COUNT, "API_ROUTE_COUNT passt nicht zu ROUTES");

esp_err_t api_handlers_register(httpd_handle_t server, command_queue_t *queue, snapshot_store_t *store)
{
    api_queue = queue;
    api_store = store;
    for (size_t i = 0; i < API_ROUTE_COUNT; i++) {
        const esp_err_t err = httpd_register_uri_handler(server, &ROUTES[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "%s nicht angemeldet (%s)", ROUTES[i].uri, esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}
