#include "mock_command_queue.h"

#include <string.h>

void mock_command_queue_init(mock_command_queue_t *queue)
{
    memset(queue, 0, sizeof(*queue));
}

bool mock_command_queue_push(mock_command_queue_t *queue, rof_command_t command)
{
    if (queue->count == MOCK_COMMAND_QUEUE_CAPACITY) {
        queue->rejected_pushes++;
        return false;
    }
    const size_t tail = (queue->head + queue->count) % MOCK_COMMAND_QUEUE_CAPACITY;
    queue->items[tail] = command;
    queue->count++;
    return true;
}

static bool queue_receive(void *ctx, rof_command_t *command)
{
    mock_command_queue_t *queue = ctx;
    if (queue->count == 0) {
        return false;
    }
    *command = queue->items[queue->head];
    queue->head = (queue->head + 1) % MOCK_COMMAND_QUEUE_CAPACITY;
    queue->count--;
    queue->received++;
    return true;
}

rof_command_source_t mock_command_queue_source(mock_command_queue_t *queue)
{
    rof_command_source_t source = { .ctx = queue, .receive = queue_receive };
    return source;
}
