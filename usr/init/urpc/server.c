#include <aos/threads.h>
#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>
#include "urpc/urpc.h"
#include "urpc/server.h"
#include "urpc/handlers.h"

//static struct thread* server_thread = NULL;
//static bool server_stop_now;

errval_t urpc_channel_init(struct urpc_channel* channel, void* fullbuffer, size_t length,
        enum urpc_channel_type channel_type){

    void* rcv_buffer=NULL;
    void* send_buffer=NULL;
    size_t buffer_size=length/2;

    if(channel_type==URPC_CHAN_MASTER){
        debug_printf("Initializing urpc channel as master\n");
        rcv_buffer=fullbuffer;
        send_buffer=fullbuffer+buffer_size;
    }else{
        debug_printf("Initializing urpc channel as slave\n");
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

    debug_printf("[URPC_SERVER] Graceful shutdown requested...\n");
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

    urpc_callback_func_t callbacks_table[URPC_OP_COUNT];
    memset(callbacks_table, 0, sizeof(callbacks_table));
    urpc_server_register_callbacks(callbacks_table);
    do {
        if (channel->server_stop_now)
        {
            debug_printf("[URPC_SERVER] Server exited without errors :)\n");
            free(message.data);
            return SYS_ERR_OK;
        }
        bool has_data = false;
        err = urpc_server_receive_try(buf, message.data, len, &message.length, &message.opcode, &has_data);
        if (err_is_fail(err))
            break;
        if (has_data)
        {
            debug_printf("SERVER: Received data length %d opcode %d\n", message.length, message.opcode);
            if (callbacks_table[message.opcode])
                (callbacks_table[message.opcode])(buf, &message);
            else
                debug_printf("[URPC_SERVER] Packet without handler!\n");
            urpc_server_dummy_answer_if_need(buf);
            // TODO: Check if we have replied!
        }
    } while (!err_is_fail(err));

    debug_printf("[URPC_SERVER] Exited with error.\n");
    assert (err_is_fail(err));
    free(message.data);
    return 0;
}
