/*
 * DNS-Antworten für das Captive Portal (NET-02, E-040). Reines C, damit
 * die Host-Tests es prüfen; die Anfragen kommen ungeprüft aus dem Netz.
 *
 * Jede Frage nach einer IPv4-Adresse (Typ A) bekommt die Adresse des
 * Access Points, egal für welchen Namen. So landet jedes Gerät, das sich
 * verbindet, auf der eigenen Seite. Andere Typen bekommen eine Antwort
 * ohne Eintrag, damit das Gerät nicht auf eine Zeitüberschreitung wartet.
 */
#ifndef DNS_MESSAGE_H
#define DNS_MESSAGE_H

#include <stddef.h>
#include <stdint.h>

#define DNS_PORT 53
#define DNS_MESSAGE_MAX 512u /* größte Nachricht über UDP ohne EDNS */
#define DNS_TYPE_A 1u
#define DNS_TYPE_AAAA 28u
#define DNS_CLASS_IN 1u
#define DNS_TTL_S 60u        /* so lange dürfen Geräte die Antwort behalten */

/* Baut zur Anfrage query (len Byte) die Antwort nach response (size Byte).
 * Liefert ihre Länge, oder 0, wenn die Anfrage verworfen wird: zu kurz,
 * kaputt, keine normale Anfrage, nicht genau eine Frage oder die Antwort
 * passt nicht in response. */
size_t dns_build_answer(const uint8_t *query, size_t len, const uint8_t ipv4[4], uint8_t *response,
                        size_t size);

#endif /* DNS_MESSAGE_H */
