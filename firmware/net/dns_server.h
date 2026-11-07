/*
 * DNS-Server für das Captive Portal (NET-02, E-040). Ein eigener Task
 * empfängt Anfragen auf UDP-Port 53 und schickt die Antwort aus
 * dns_message zurück. Er kennt die Regelung nicht.
 */
#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include "esp_err.h"

#define DNS_SERVER_PRIORITY 4u /* unter dem HTTP-Server (5), weit unter dem Regeltask (10) */
#define DNS_SERVER_STACK_BYTES 3072u
#define DNS_SERVER_CORE 0      /* CPU 1 bleibt dem Regeltask */

/* Öffnet den Port und startet den Task. Erst nach wifi_ap_start() aufrufen. */
esp_err_t dns_server_start(void);

#endif /* DNS_SERVER_H */
