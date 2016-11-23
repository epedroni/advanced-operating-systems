
#include <aos/aos.h>
#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>
#include "urpc/urpc.h"
#include "urpc/opcodes.h"

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

static errval_t read_from_buffer(struct urpc_buffer_data* data, void* cpyto, size_t destlen, size_t* datalen, uint32_t* opcode)
{
    *datalen = data->data_len;
    *opcode = data->opcode;
    if (data->opcode >= URPC_OP_COUNT)
        return URPC_ERR_INVALID_OPCODE;
    if (*datalen > destlen)
        return URPC_ERR_BUFFER_TOO_SMALL;
    memcpy(cpyto, (void*)data->data, *datalen);
    return SYS_ERR_OK;
}

errval_t urpc_server_receive_try(struct urpc_buffer* urpc, void* buf, size_t len, size_t* datalen, uint32_t* opcode, bool* has_data)
{
    if (!urpc->is_server)
        return URPC_ERR_IS_NOT_SERVER_BUFFER;

    if (urpc->buffer->status == URPC_CLIENT_SENT_DATA)
    {
        *has_data = true;
        return read_from_buffer(urpc->buffer, buf, len, datalen, opcode);
    }

    return SYS_ERR_OK;
}

errval_t urpc_server_receive_block(struct urpc_buffer* urpc, void* buf, size_t len, size_t* datalen, uint32_t* opcode)
{
    if (!urpc->is_server)
        return URPC_ERR_IS_NOT_SERVER_BUFFER;
    while (urpc->buffer->status != URPC_CLIENT_SENT_DATA);
    return read_from_buffer(urpc->buffer, buf, len, datalen, opcode);
}

static void dump_buffer_begin(const char* from, char* buf)
{
    debug_printf("%s: Dump buffer: 0x%x 0x%x 0x%x 0x%x...\n", from, buf[0], buf[1], buf[2], buf[3]);
}
errval_t urpc_client_send(struct urpc_buffer* urpc, uint32_t opcode, void* data, size_t len, void** answer, size_t* answer_len)
{
    *answer_len = 0;

    if (urpc->is_server)
        return URPC_ERR_IS_NOT_CLIENT_BUFFER;
    if (urpc->buffer->status != URPC_NO_DATA)
        return URPC_ERR_WRONG_BUFFER_STATUS;
    if (URPC_MAX_DATA_SIZE(urpc) < len)
        return URPC_ERR_BUFFER_TOO_SMALL;
    if (opcode >= URPC_OP_COUNT)
        return URPC_ERR_INVALID_OPCODE;

    // 1. Send data
    memcpy(urpc->buffer->data, data, len);
    urpc->buffer->data_len = len;
    urpc->buffer->opcode = opcode;
    dmb();
    urpc->buffer->status = URPC_CLIENT_SENT_DATA;
    dmb();

    // 2. Wait for answer
    while (urpc->buffer->status == URPC_CLIENT_SENT_DATA);

    if (urpc->buffer->status == URPC_SERVER_REPLIED_ERROR)
    {
        // VoooodoooOOooo
        void* pbuf = (void*)&urpc->buffer->data[0];
        errval_t err = *((errval_t*)pbuf);
        if (err_is_fail(err))
            return err;
    }

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
    dump_buffer_begin("urpc_client_send(answer)", urpc->buffer->data);
    *answer_len=data_len;

    // 4. Once we have finished everything, we are ready to send new data
    dmb();
    urpc->buffer->status = URPC_NO_DATA;
    debug_printf("Finished client send data. Answer is at address 0x%08x\n", (int)*answer);
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

    if (len)
        memcpy(urpc->buffer->data, data, len);
    urpc->buffer->data_len = len;
    dump_buffer_begin("urpc_server_answer", urpc->buffer->data);

    dmb();
    urpc->buffer->status = URPC_SERVER_REPLIED_DATA;
    return SYS_ERR_OK;
}

errval_t urpc_server_answer_error(struct urpc_buffer* urpc, errval_t error)
{
    if (!urpc->is_server)
        return URPC_ERR_IS_NOT_SERVER_BUFFER;
    if (urpc->buffer->status != URPC_CLIENT_SENT_DATA)
        return URPC_ERR_WRONG_BUFFER_STATUS;

    memcpy(urpc->buffer->data, &error, sizeof(error));
    urpc->buffer->data_len = sizeof(error);

    dmb();
    urpc->buffer->status = URPC_SERVER_REPLIED_ERROR;

    debug_printf("Server replied with data\n");
    return SYS_ERR_OK;
}

errval_t urpc_server_dummy_answer_if_need(struct urpc_buffer* urpc)
{
    if (urpc->buffer->status == URPC_CLIENT_SENT_DATA)
        return urpc_server_answer(urpc, NULL, 0);
    return SYS_ERR_OK;
}
