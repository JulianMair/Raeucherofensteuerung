/*
 * Log-Interface (ARC-02). core schreibt Meldungen nur hierüber,
 * nie direkt auf eine Konsole.
 */
#ifndef ROF_LOG_H
#define ROF_LOG_H

typedef enum {
    ROF_LOG_ERROR = 0,
    ROF_LOG_WARN,
    ROF_LOG_INFO,
    ROF_LOG_DEBUG,
    ROF_LOG_LEVEL_COUNT /* Anzahl der Stufen, keine eigene Stufe */
} rof_log_level_t;

typedef struct {
    void *ctx;
    /* msg ist ein fertiger Text ohne Zeilenumbruch. Er gilt nur während
     * des Aufrufs, eine Implementierung muss ihn bei Bedarf kopieren. */
    void (*write)(void *ctx, rof_log_level_t level, const char *msg);
} rof_log_t;

static inline void rof_log_write(const rof_log_t *logger, rof_log_level_t level, const char *msg)
{
    logger->write(logger->ctx, level, msg);
}

#endif /* ROF_LOG_H */
