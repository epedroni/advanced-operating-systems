#ifndef _HEADER_INIT_URPC_SERVER
#define _HEADER_INIT_URPC_SERVER

#include <aos/threads.h>
#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>
#include "urpc/urpc.h"
#include "urpc/opcodes.h"

struct urpc_buffer;

enum urpc_channel_type{
    URPC_CHAN_MASTER=0,   //For core 0
    URPC_CHAN_SLAVE=1     //For spawned cores
};

typedef errval_t (*urpc_callback_func_t)(struct urpc_buffer*, struct urpc_message*, void* context);

struct urpc_message_closure{
    urpc_callback_func_t message_handler;
    void* context;
};

struct urpc_channel{
    struct urpc_buffer buffer_send;
    struct urpc_buffer buffer_rcv;

    struct thread* server_thread;
    bool server_stop_now;

    struct urpc_message_closure callbacks_table[URPC_OP_COUNT];
};

errval_t urpc_server_start_listen(struct urpc_channel* channel, bool new_thread);
errval_t urpc_server_stop(struct urpc_channel* channel);
int urpc_server_event_loop(void* data);

errval_t urpc_server_register_handler(struct urpc_channel* channel, enum urpc_opcodes opcode,
        urpc_callback_func_t message_handler, void* context);
errval_t urpc_channel_init(struct urpc_channel* channel, void* fullbuffer, size_t length,
        enum urpc_channel_type);

#endif
