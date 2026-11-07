/*
 * HTTP-Server (API-02, API-03, NET-02, NET-03, E-039).
 *
 * esp_http_server mit einem Task auf CPU 0, Priorität 5, also unter dem
 * Regeltask. Er liefert die Weboberfläche gzip-komprimiert aus LittleFS.
 * Unbekannte Pfade außerhalb von /api leitet er auf die Startseite um; so
 * öffnen Handys die Seite beim Verbinden (Captive Portal).
 */
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "command_queue.h"
#include "snapshot_store.h"

#include "esp_err.h"

#define HTTP_SERVER_PRIORITY 5u
#define HTTP_SERVER_CORE 0         /* CPU 1 bleibt dem Regeltask */
#define HTTP_SERVER_STACK_BYTES 6144u
#define HTTP_SERVER_MAX_SOCKETS 8  /* braucht 3 mehr in lwIP, siehe sdkconfig.defaults */

/* Hängt LittleFS ein, startet den Server und meldet API und Dateien an.
 * Fehlt LittleFS, läuft der Server trotzdem, nur ohne Weboberfläche. Die
 * Zeiger müssen gültig bleiben. */
esp_err_t http_server_start(command_queue_t *queue, snapshot_store_t *store);

#endif /* HTTP_SERVER_H */
