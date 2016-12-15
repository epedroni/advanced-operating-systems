
#include <aos/aos.h>
#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>
#include <aos/urpc/urpc.h>

errval_t urpc_server_init(struct urpc_buffer* urpc, void* buffer, size_t length)
{
    urpc->is_server = true;
    urpc->buffer_len = length;
    urpc->buffer = buffer;
    thread_mutex_init(&urpc->buff_lock);
    if (urpc->buffer_len < URPC_BUF_HEADER_LENGTH)
        return URPC_ERR_BUFFER_TOO_SMALL;
    return SYS_ERR_OK;
}

errval_t urpc_client_init(struct urpc_buffer* urpc, void* buffer, size_t length)
{
    urpc->is_server = false;
    urpc->buffer_len = length;
    urpc->buffer = buffer;
    thread_mutex_init(&urpc->buff_lock);
    if (urpc->buffer_len < URPC_BUF_HEADER_LENGTH)
        return URPC_ERR_BUFFER_TOO_SMALL;
    return SYS_ERR_OK;
}

static errval_t read_from_buffer(struct urpc_buffer_data* data, void* cpyto, size_t destlen, size_t* datalen, uint32_t* opcode)
{
    *datalen = data->data_len;
    *opcode = data->opcode;
//  We will remove this
//    if (data->opcode >= URPC_OP_COUNT)
//        return URPC_ERR_INVALID_OPCODE;
    if (*datalen > destlen)
    {
        debug_printf("read_from_buffer: destlen=%d datalen=%d\n", destlen, *datalen);
        return URPC_ERR_BUFFER_TOO_SMALL;
    }
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
    while (urpc->buffer->status != URPC_CLIENT_SENT_DATA){
        thread_yield();
    }
    return read_from_buffer(urpc->buffer, buf, len, datalen, opcode);
}

static errval_t client_send_and_wait(struct urpc_buffer* urpc, uint32_t opcode, void* data, size_t len)
{
    if (urpc->is_server)
        return URPC_ERR_IS_NOT_CLIENT_BUFFER;
    if (urpc->buffer->status != URPC_NO_DATA)
        return URPC_ERR_WRONG_BUFFER_STATUS;
    if (URPC_MAX_DATA_SIZE(urpc) < len)
        return URPC_ERR_URPC_BUFFER_TOO_SMALL_FOR_SEND;
    //  We will remove this
    //    if (data->opcode >= URPC_OP_COUNT)
    //        return URPC_ERR_INVALID_OPCODE;

    // 1. Send data
    thread_mutex_lock(&urpc->buff_lock);
    memcpy(urpc->buffer->data, data, len);
    urpc->buffer->data_len = len;
    urpc->buffer->opcode = opcode;
    dmb();
    urpc->buffer->status = URPC_CLIENT_SENT_DATA;
    dmb();

    // 2. Wait for answer
    while (urpc->buffer->status == URPC_CLIENT_SENT_DATA){
        thread_yield();
    }

    if (urpc->buffer->status == URPC_SERVER_REPLIED_ERROR)
    {
        // VoooodoooOOooo
        void* pbuf = (void*)&urpc->buffer->data[0];
        errval_t err = *((errval_t*)pbuf);
        if (err_is_fail(err))
        {
            urpc->buffer->status = URPC_NO_DATA;
            thread_mutex_unlock(&urpc->buff_lock);
            return err;
        }
    }
    // TODO: Invalidate cache? Or not needed?
    //assert(false && "TODO: Clear cache here??");
    thread_mutex_unlock(&urpc->buff_lock);
    return SYS_ERR_OK;
}

static errval_t client_send_chunk_and_wait(struct urpc_buffer* urpc, uint32_t opcode, void* data, size_t len,
        bool first_message, bool final_message)
{
    if (urpc->is_server)
        return URPC_ERR_IS_NOT_CLIENT_BUFFER;
    if ((urpc->buffer->status != URPC_NO_DATA) && (urpc->buffer->status != URPC_CLIENT_SENT_DATA))
        return URPC_ERR_WRONG_BUFFER_STATUS;
    if (URPC_MAX_DATA_SIZE(urpc) < len)
        return URPC_ERR_URPC_BUFFER_TOO_SMALL_FOR_SEND;
    //  We will remove this
    //    if (data->opcode >= URPC_OP_COUNT)
    //        return URPC_ERR_INVALID_OPCODE;

    if(first_message){
        thread_mutex_lock(&urpc->buff_lock);
        urpc->buffer->data_len=0;
    }

    // 1. Send data
    memcpy(urpc->buffer->data+urpc->buffer->data_len, data, len);
    urpc->buffer->data_len += len;

    // This is not a final message, there will be more
    if(!final_message){
        return SYS_ERR_OK;
    }

    urpc->buffer->opcode = opcode;
    dmb();
    urpc->buffer->status = URPC_CLIENT_SENT_DATA;
    dmb();

    // 2. Wait for answer
    while (urpc->buffer->status == URPC_CLIENT_SENT_DATA){
        thread_yield();
    }

    if (urpc->buffer->status == URPC_SERVER_REPLIED_ERROR)
    {
        // VoooodoooOOooo
        void* pbuf = (void*)&urpc->buffer->data[0];
        errval_t err = *((errval_t*)pbuf);
        if (err_is_fail(err))
        {
            dmb();
            urpc->buffer->status = URPC_NO_DATA;
            thread_mutex_unlock(&urpc->buff_lock);
            return err;
        }
    }
    // TODO: Invalidate cache? Or not needed?
    //assert(false && "TODO: Clear cache here??");
    thread_mutex_unlock(&urpc->buff_lock);
    return SYS_ERR_OK;
}

errval_t urpc_client_send_chunck(struct urpc_buffer* urpc, void* data, size_t len, bool first_chunck){
    ERROR_RET1(client_send_chunk_and_wait(urpc, 0, data, len, first_chunck, false));

    return SYS_ERR_OK;
}

errval_t urpc_client_send_final_chunck_receive_fixed_size(struct urpc_buffer* urpc, uint32_t opcode,
        void* data, size_t len, void* answer, size_t answer_size, size_t* actual_answer_size){

    ERROR_RET1(client_send_chunk_and_wait(urpc, opcode, data, len, false, true));

    // 3. Copy answer
    size_t data_len = urpc->buffer->data_len;
    if (actual_answer_size)
        *actual_answer_size = data_len;
    if (data_len > answer_size)
    {
        urpc->buffer->status = URPC_NO_DATA;
        return URPC_ERR_ANSWER_BUFFER_TOO_SMALL;
    }
    if (URPC_MAX_DATA_SIZE(urpc) < data_len)
    {
        urpc->buffer->status = URPC_NO_DATA;
        return URPC_ERR_PROTOCOL_FATAL_ERROR;
    }
    memcpy(answer, urpc->buffer->data, data_len);

    // 4. Once we have finished everything, we are ready to send new data
    dmb();
    urpc->buffer->status = URPC_NO_DATA;
    return SYS_ERR_OK;
}

errval_t urpc_client_send(struct urpc_buffer* urpc, uint32_t opcode, void* data, size_t len, void** answer, size_t* answer_len)
{
    *answer_len = 0;

    ERROR_RET1(client_send_and_wait(urpc, opcode, data, len));

    // 3. Copy answer
    size_t data_len = urpc->buffer->data_len;
    if (URPC_MAX_DATA_SIZE(urpc) < data_len)
    {
        urpc->buffer->status = URPC_NO_DATA;
        return URPC_ERR_PROTOCOL_FATAL_ERROR;
    }
    *answer = malloc(data_len);
    memcpy(*answer, urpc->buffer->data, data_len);
    *answer_len=data_len;

    // 4. Once we have finished everything, we are ready to send new data
    dmb();
    urpc->buffer->status = URPC_NO_DATA;
    return SYS_ERR_OK;
}

errval_t urpc_client_send_receive_fixed_size(struct urpc_buffer* urpc, uint32_t opcode,
        void* data, size_t len, void* answer, size_t answer_size, size_t* actual_answer_size)
{
    ERROR_RET1(client_send_and_wait(urpc, opcode, data, len));

    // 3. Copy answer
    size_t data_len = urpc->buffer->data_len;
    if (actual_answer_size)
        *actual_answer_size = data_len;
    if (data_len > answer_size)
    {
        urpc->buffer->status = URPC_NO_DATA;
        return URPC_ERR_ANSWER_BUFFER_TOO_SMALL;
    }
    if (URPC_MAX_DATA_SIZE(urpc) < data_len)
    {
        urpc->buffer->status = URPC_NO_DATA;
        return URPC_ERR_PROTOCOL_FATAL_ERROR;
    }
    memcpy(answer, urpc->buffer->data, data_len);

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
    {
        debug_printf("urpc_server_answer[URPC_ERR_BUFFER_TOO_SMALL_FOR_ANSWER]\n");
        debug_printf("URPC Max Capacity: %d | Answer size: %d\n", URPC_MAX_DATA_SIZE(urpc), len);
        return URPC_ERR_BUFFER_TOO_SMALL_FOR_ANSWER;
    }

    if (len)
        memcpy(urpc->buffer->data, data, len);
    urpc->buffer->data_len = len;

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
