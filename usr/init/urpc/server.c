#include <aos/threads.h>
#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>
#include "urpc/urpc.h"
#include "urpc/server.h"
#include "urpc/handlers.h"

static struct thread* server_thread = NULL;
static bool server_stop_now;

errval_t urpc_server_start_listen(struct urpc_buffer* buf, bool new_thread)
{
    if (new_thread)
    {
        server_stop_now = false;
        dmb();
        server_thread = thread_create(urpc_server_event_loop, buf);
    }
    else
        urpc_server_event_loop(buf);
    return SYS_ERR_OK;
}

errval_t urpc_server_stop(void)
{
    // TODO: There is a race condition with $server_thread pointer
    if (!server_thread)
        return SYS_ERR_OK;

    debug_printf("[URPC_SERVER] Graceful shutdown requested...\n");
    int retval;
    server_stop_now = true;
    errval_t join_err = thread_join(server_thread, &retval);
    server_thread = NULL; // (Object deleted at thread exit)
    return join_err;
}

int urpc_server_event_loop(void* _buf_void)
{
    struct urpc_buffer* buf = _buf_void;
    errval_t err;
    size_t len = 1024;
    struct urpc_message message;
    message.data = malloc(len);
    assert(message.data);

    urpc_callback_func_t callbacks_table[URPC_OP_COUNT];
    memset(callbacks_table, 0, sizeof(callbacks_table));
    urpc_server_register_callbacks(callbacks_table);
    do {
        if (server_stop_now)
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
