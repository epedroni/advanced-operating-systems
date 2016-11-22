#include <aos/threads.h>
#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>
#include "urpc.h"
#include "urpc_server.h"

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
    size_t data_len;
    void* data = malloc(len);
    do {
        if (server_stop_now)
        {
            debug_printf("[URPC_SERVER] Server exited without errors :)\n");
            return SYS_ERR_OK;
        }
        bool has_data = false;
        err = urpc_server_receive_try(buf, data, len, &data_len, &has_data);
        if (err_is_fail(err))
            break;
        if (has_data)
        {
            debug_printf("SERVER: Received data length %d\n", data_len);
            char* data_as_str = data;
            data_as_str[data_len] = 0;
            for (int i = 0; i < data_len; ++i)
                debug_printf("%d: 0x%02x [%c]\n", (int)i, (int)data_as_str[i], data_as_str[i]);
            debug_printf("SERVER: Data as string: \"%s\"\n", data_as_str);
            char answer[] = "Hello AOS World!";
            err = urpc_server_answer(buf, answer, sizeof(answer));
            // TODO: Process data
        }
    } while (!err_is_fail(err));

    debug_printf("[URPC_SERVER] Exited with error.\n");
    assert (err_is_fail(err));
    return 0;
}
