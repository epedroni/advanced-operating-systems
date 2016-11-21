
#include <aos/aos.h>
#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>
#include "urpc.h"

errval_t urpc_server_init(struct urpc_buffer* urpc, void* buffer, size_t length)
{
    urpc->is_server = true;
    urpc->buffer_len = length;
    urpc->buffer = buffer;
    if (urpc->buffer_len < URPC_BUF_HEADER_LENGTH)
        return URPC_ERR_BUFFER_TOO_SMALL;
    return SYS_ERR_OK;
}

errval_t urpc_client_init(struct urpc_buffer* urpc, void* buffer, size_t length)
{
    urpc->is_server = false;
    urpc->buffer_len = length;
    urpc->buffer = buffer;
    if (urpc->buffer_len < URPC_BUF_HEADER_LENGTH)
        return URPC_ERR_BUFFER_TOO_SMALL;
    return SYS_ERR_OK;
}

errval_t urpc_server_read(struct urpc_buffer* urpc)
{
    if (!urpc->is_server)
        return URPC_ERR_IS_NOT_SERVER_BUFFER;
    while (urpc->buffer->status != URPC_CLIENT_SENT_DATA);
    debug_printf("urpc_server_read: got data\n");
    return SYS_ERR_OK;
}

errval_t urpc_client_send(struct urpc_buffer* urpc, void* data, size_t len, void** answer, size_t* answer_len)
{
    *answer_len = 0;

    if (urpc->is_server)
        return URPC_ERR_IS_NOT_CLIENT_BUFFER;
    if (urpc->buffer->status != URPC_NO_DATA)
        return URPC_ERR_WRONG_BUFFER_STATUS;
    if (URPC_MAX_DATA_SIZE(urpc) < len)
        return URPC_ERR_BUFFER_TOO_SMALL;

    // 1. Send data
    memcpy(urpc->buffer->data, data, len);
    urpc->buffer->data_len = len;
    dmb();
    urpc->buffer->status = URPC_CLIENT_SENT_DATA;
    dmb();
    // 2. Wait for answer
    while (urpc->buffer->status != URPC_SERVER_REPLIED_DATA);
    debug_printf("urpc_client_send: received response\n");
    // 3. Copy answer
    // TODO: Invalidate cache? Or not needed?
    //assert(false && "TODO: Clear cache here??");
    size_t data_len = urpc->buffer->data_len;
    if (URPC_MAX_DATA_SIZE(urpc) < data_len)
    {
        urpc->buffer->status = URPC_NO_DATA;
        return URPC_ERR_PROTOCOL_FATAL_ERROR;
    }
    *answer = malloc(data_len);
    memcpy(*answer, urpc->buffer->data, data_len);

    // 4. Once we have finished everything, we are ready to send new data
    dmb();
    urpc->buffer->status = URPC_NO_DATA;
    return SYS_ERR_OK;
}

errval_t urpc_server_answer(struct urpc_buffer* urpc, void* data, size_t len)
{
    if (!urpc->is_server)
        return URPC_ERR_IS_NOT_SERVER_BUFFER;
    if (urpc->buffer->status != URPC_CLIENT_SENT_DATA)
        return URPC_ERR_WRONG_BUFFER_STATUS;
    if (URPC_MAX_DATA_SIZE(urpc) < len)
        return URPC_ERR_BUFFER_TOO_SMALL;

    memcpy(urpc->buffer->data, data, len);
    urpc->buffer->data_len = len;

    dmb();
    urpc->buffer->status = URPC_SERVER_REPLIED_DATA;

    debug_printf("Server replied with data\n");
    return SYS_ERR_OK;
}
