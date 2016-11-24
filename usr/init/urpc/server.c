#include "urpc/server.h"

#define URPC_SERV_DEBUG(...) debug_printf(__VA_ARGS__)

errval_t urpc_server_register_handler(struct urpc_channel* channel, enum urpc_opcodes opcode, urpc_callback_func_t message_handler,
        void* context){

    URPC_SERV_DEBUG("########## REGISTER HANDLER FOR OPCODE %d\n", opcode);
    channel->callbacks_table[opcode].message_handler=message_handler;
    channel->callbacks_table[opcode].context=context;

    return SYS_ERR_OK;
}

errval_t urpc_channel_init(struct urpc_channel* channel, void* fullbuffer, size_t length,
        enum urpc_channel_type channel_type){

    void* rcv_buffer=NULL;
    void* send_buffer=NULL;
    size_t buffer_size=length/2;

    memset(channel->callbacks_table, 0, sizeof(channel->callbacks_table));

    if(channel_type==URPC_CHAN_MASTER){
        URPC_SERV_DEBUG("Initializing urpc channel as master\n");
        rcv_buffer=fullbuffer;
        send_buffer=fullbuffer+buffer_size;
    }else{
        URPC_SERV_DEBUG("Initializing urpc channel as slave\n");
        rcv_buffer=fullbuffer+buffer_size;
        send_buffer=fullbuffer;
    }

    urpc_server_init(&channel->buffer_rcv, rcv_buffer, buffer_size);
    urpc_client_init(&channel->buffer_send, send_buffer, buffer_size);

    channel->server_thread=NULL;
    channel->server_stop_now=false;

    return SYS_ERR_OK;
}

errval_t urpc_server_start_listen(struct urpc_channel* channel, bool new_thread)
{
    if (new_thread)
    {
        assert(!channel->server_thread);
        channel->server_stop_now=false;
        dmb();
        channel->server_thread = thread_create(urpc_server_event_loop, channel);
    }
    else
        urpc_server_event_loop(channel);
    return SYS_ERR_OK;
}

errval_t urpc_server_stop(struct urpc_channel* channel)
{
    // TODO: There is a race condition with $server_thread pointer
    if (!channel->server_thread)
        return SYS_ERR_OK;

    URPC_SERV_DEBUG("[URPC_SERVER] Graceful shutdown requested...\n");
    int retval;
    channel->server_stop_now = true;
    errval_t join_err = thread_join(channel->server_thread, &retval);
    channel->server_thread = NULL; // (Object deleted at thread exit)
    return join_err;
}

int urpc_server_event_loop(void* _buf_void)
{
    struct urpc_channel* channel=_buf_void;
    struct urpc_buffer* buf = &channel->buffer_rcv;
    errval_t err;
    size_t len = 1024;
    struct urpc_message message;
    message.data = malloc(len);
    assert(message.data);

    do {
        if (channel->server_stop_now)
        {
            URPC_SERV_DEBUG("[URPC_SERVER] Server exited without errors :)\n");
            free(message.data);
            return SYS_ERR_OK;
        }
        bool has_data = false;
        err = urpc_server_receive_try(buf, message.data, len, &message.length, &message.opcode, &has_data);
        if (err_is_fail(err))
            break;
        if (has_data)
        {
            URPC_SERV_DEBUG("SERVER: Received data length %d opcode %d\n", message.length, message.opcode);
            if (channel->callbacks_table[message.opcode].message_handler)
            {
                errval_t cb_error = (channel->callbacks_table[message.opcode].message_handler)(buf, &message,
                        channel->callbacks_table[message.opcode].context);
                urpc_server_answer_error(buf, cb_error);
            }
            else
            {
                URPC_SERV_DEBUG("[URPC_SERVER] Packet without handler!\n");
                urpc_server_answer_error(buf, URPC_ERR_NO_HANDLER_FOR_OPCODE);
            }
            // TODO: Check if we have replied!
        }
    } while (!err_is_fail(err));

    URPC_SERV_DEBUG("[URPC_SERVER] Exited with error.\n");
    assert (err_is_fail(err));
    free(message.data);
    return 0;
}
