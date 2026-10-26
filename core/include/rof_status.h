/*
 * Gemeinsame Rückgabewerte aller Interfaces.
 */
#ifndef ROF_STATUS_H
#define ROF_STATUS_H

typedef enum {
    ROF_OK = 0,
    ROF_ERR_NOT_FOUND,   /* Sensor oder Schlüssel nicht vorhanden */
    ROF_ERR_IO,          /* Lesen oder Schreiben fehlgeschlagen, z. B. CRC-Fehler oder Zeitüberschreitung */
    ROF_ERR_INVALID_ARG, /* Parameter außerhalb des erlaubten Bereichs */
    ROF_ERR_NO_SPACE     /* Puffer oder Tabelle zu klein */
} rof_status_t;

#endif /* ROF_STATUS_H */
