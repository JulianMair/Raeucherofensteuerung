#include "command_queue.h"

void command_queue_init(command_queue_t *queue)
{
    /* Mit eigenem Speicher kann das Anlegen nicht scheitern */
    queue->handle = xQueueCreateStatic(COMMAND_QUEUE_LENGTH, sizeof(rof_command_t), queue->storage,
                                       &queue->control);
}

bool command_queue_send(command_queue_t *queue, const rof_command_t *command)
{
    return xQueueSend(queue->handle, command, 0) == pdTRUE;
}

static bool queue_receive(void *ctx, rof_command_t *command)
{
    const command_queue_t *queue = ctx;
    return xQueueReceive(queue->handle, command, 0) == pdTRUE;
}

rof_command_source_t command_queue_source(command_queue_t *queue)
{
    return (rof_command_source_t){ .ctx = queue, .receive = queue_receive };
}
