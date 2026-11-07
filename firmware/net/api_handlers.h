/*
 * REST-API nach docs/api.yaml (API-02, ARC-04 bis ARC-07, E-028, E-039).
 *
 * Jeder Befehl: Inhalt lesen, mit proto prüfen, laufende Nummer vergeben,
 * ohne Warten in die Befehlsqueue, dann im Snapshot auf das Ergebnis dieser
 * Nummer warten, höchstens API_RESULT_TIMEOUT_MS. Abfragen lesen nur die
 * Snapshot-Kopie. Mit der Regelung spricht dieses Modul ausschließlich über
 * rof_exchange und proto, nie über core oder Treiber.
 */
#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include "command_queue.h"
#include "snapshot_store.h"

#include "esp_err.h"
#include "esp_http_server.h"

#define API_RESULT_TIMEOUT_MS 2000u /* danach 504, so steht es in der API */
#define API_RESULT_POLL_MS 50u      /* so oft wird im Snapshot nachgesehen */
#define API_SNAPSHOT_WAIT_MS 100u   /* längste Wartezeit auf den Mutex */

/* Meldet alle Pfade unter /api beim Server an. Die Zeiger müssen gültig bleiben. */
esp_err_t api_handlers_register(httpd_handle_t server, command_queue_t *queue, snapshot_store_t *store);

#endif /* API_HANDLERS_H */
