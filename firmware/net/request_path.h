/*
 * Pfade der Anfragen zerlegen (API-02, NET-02, NET-03, E-039). Reines C,
 * damit die Host-Tests es prüfen. Es greift weder auf Dateien noch auf das
 * Netzwerk zu. Der Abfrageteil ab "?" gehört nie zum Pfad.
 */
#ifndef REQUEST_PATH_H
#define REQUEST_PATH_H

#include "rof_proto.h"

#include <stdbool.h>
#include <stddef.h>

#define NET_URI_PATH_MAX 64u /* längster Pfad einer Datei, ohne Abfrageteil */
#define NET_WWW_ROOT "/www"  /* hier ist LittleFS eingehängt */
/* "/www" + Pfad + "index.html.gz" mit abschließender Null */
#define NET_FILE_PATH_MAX (sizeof(NET_WWW_ROOT) - 1u + NET_URI_PATH_MAX + sizeof("index.html.gz"))

typedef enum {
    NET_SENSOR_PATH_NO_MATCH = 0, /* kein Sensorpfad */
    NET_SENSOR_PATH_ROLE,         /* /api/sensors/{id}/role */
    NET_SENSOR_PATH_OFFSET        /* /api/sensors/{id}/offset */
} net_sensor_path_t;

/* Erkennt /api/sensors/{id}/role und /api/sensors/{id}/offset. Die ID kommt
 * als Text nach id, wenn sie genau 16 Zeichen hat, sonst bleibt id leer.
 * Ob sie gültig ist, prüft proto (bad_sensor_id). So wird nie eine zu
 * lange ID abgeschnitten und sieht dadurch gültig aus. */
net_sensor_path_t net_parse_sensor_path(const char *uri, char id[ROF_PROTO_SENSOR_ID_LEN + 1]);

/* true für /api und alles darunter. Solche Pfade werden nie umgeleitet. */
bool net_is_api_path(const char *uri);

typedef struct {
    char path[NET_FILE_PATH_MAX]; /* gzip-Datei in LittleFS, z. B. "/www/index.html.gz" */
    const char *content_type;
} net_static_file_t;

/* Ordnet einen Pfad seiner gzip-Datei zu: "/" wird "/www/index.html.gz",
 * "/app.js" wird "/www/app.js.gz". Erlaubt sind nur Buchstaben a bis z,
 * Ziffern, ".", "_", "-" und "/"; abgelehnt werden "..", "//" und Pfade über
 * NET_URI_PATH_MAX Zeichen. */
bool net_static_file_for(const char *uri, net_static_file_t *file);

/* Content-Type nach der Endung vor ".gz" */
const char *net_content_type(const char *path);

#endif /* REQUEST_PATH_H */
