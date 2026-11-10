#include "request_path.h"

#include <stdio.h>
#include <string.h>

static const char SENSOR_PREFIX[] = "/api/sensors/";

/* Länge des Pfads ohne Abfrageteil */
static size_t path_length(const char *uri)
{
    return strcspn(uri, "?");
}

static bool field_is(const char *field, size_t len, const char *name)
{
    return len == strlen(name) && strncmp(field, name, len) == 0;
}

net_sensor_path_t net_parse_sensor_path(const char *uri, char id[ROF_PROTO_SENSOR_ID_LEN + 1])
{
    const size_t len = path_length(uri);
    const size_t prefix_len = sizeof(SENSOR_PREFIX) - 1;
    if (len <= prefix_len || strncmp(uri, SENSOR_PREFIX, prefix_len) != 0) {
        return NET_SENSOR_PATH_NO_MATCH;
    }
    const char *id_start = uri + prefix_len;
    const char *slash = memchr(id_start, '/', len - prefix_len);
    if (slash == NULL || slash == id_start) {
        return NET_SENSOR_PATH_NO_MATCH;
    }
    const char *field = slash + 1;
    const size_t field_len = (size_t)(uri + len - field);
    net_sensor_path_t kind;
    if (field_is(field, field_len, "role")) {
        kind = NET_SENSOR_PATH_ROLE;
    } else if (field_is(field, field_len, "offset")) {
        kind = NET_SENSOR_PATH_OFFSET;
    } else {
        return NET_SENSOR_PATH_NO_MATCH;
    }
    const size_t id_len = (size_t)(slash - id_start);
    if (id_len == ROF_PROTO_SENSOR_ID_LEN) {
        memcpy(id, id_start, id_len);
        id[id_len] = '\0';
    } else {
        id[0] = '\0';
    }
    return kind;
}

bool net_is_api_path(const char *uri)
{
    const size_t len = path_length(uri);
    return len >= 4 && strncmp(uri, "/api", 4) == 0 && (len == 4 || uri[4] == '/');
}

static bool allowed_char(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' ||
           ch == '_' || ch == '-' || ch == '/';
}

/* Kommt das Zeichenpaar first second irgendwo vor? */
static bool has_pair(const char *text, size_t len, char first, char second)
{
    for (size_t i = 0; i + 1 < len; i++) {
        if (text[i] == first && text[i + 1] == second) {
            return true;
        }
    }
    return false;
}

bool net_static_file_for(const char *uri, net_static_file_t *file)
{
    const size_t len = path_length(uri);
    if (len == 0 || len > NET_URI_PATH_MAX || uri[0] != '/') {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (!allowed_char(uri[i])) {
            return false;
        }
    }
    /* ".." könnte aus /www herausführen, "//" ergibt einen leeren Namen */
    if (has_pair(uri, len, '.', '.') || has_pair(uri, len, '/', '/')) {
        return false;
    }
    const char *index = uri[len - 1] == '/' ? "index.html" : "";
    const int written =
        snprintf(file->path, sizeof(file->path), "%s%.*s%s.gz", NET_WWW_ROOT, (int)len, uri, index);
    if (written < 0 || (size_t)written >= sizeof(file->path)) {
        return false; /* kommt bei NET_FILE_PATH_MAX nicht vor */
    }
    file->content_type = net_content_type(file->path);
    return true;
}

const char *net_content_type(const char *path)
{
    static const struct {
        const char *extension;
        const char *type;
    } types[] = {
        { ".html", "text/html; charset=utf-8" },
        { ".js", "application/javascript" },
        { ".css", "text/css" },
        { ".svg", "image/svg+xml" },
        { ".json", "application/json" },
        { ".ico", "image/x-icon" },
        { ".png", "image/png" },
        { ".woff2", "font/woff2" },
    };
    size_t len = strlen(path);
    if (len >= 3 && strcmp(path + len - 3, ".gz") == 0) {
        len -= 3;
    }
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        const size_t ext_len = strlen(types[i].extension);
        if (len >= ext_len && strncmp(path + len - ext_len, types[i].extension, ext_len) == 0) {
            return types[i].type;
        }
    }
    return "application/octet-stream";
}

/* ---- Herkunft der Befehle (E-053) ----------------------------------------- */

bool net_origin_allowed(const char *origin, const char *host)
{
    if (origin == NULL || origin[0] == '\0') {
        return true; /* kein Browser, z. B. curl */
    }
    static const char SCHEME[] = "http://";
    const size_t scheme_len = sizeof(SCHEME) - 1u;
    if (host == NULL || host[0] == '\0' || strncmp(origin, SCHEME, scheme_len) != 0) {
        return false;
    }
    /* Der Rest muss genau der Host sein, mit Port, falls einer genannt ist */
    return strcmp(origin + scheme_len, host) == 0;
}
