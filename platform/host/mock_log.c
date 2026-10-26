#include "mock_log.h"

#include <stdio.h>
#include <string.h>

static void mock_write(void *ctx, rof_log_level_t level, const char *msg)
{
    mock_log_t *mock = ctx;
    mock->count[level]++;
    mock->last_level = level;
    /* snprintf kürzt bei Bedarf und schließt immer mit einer Null ab. */
    snprintf(mock->last_msg, sizeof(mock->last_msg), "%s", msg);
}

void mock_log_init(mock_log_t *mock)
{
    memset(mock, 0, sizeof(*mock));
}

rof_log_t mock_log_interface(mock_log_t *mock)
{
    rof_log_t logger = { .ctx = mock, .write = mock_write };
    return logger;
}
