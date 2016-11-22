#ifndef _HEADER_INIT_URPC_SERVER
#define _HEADER_INIT_URPC_SERVER

#include <aos/aos.h>

struct urpc_buffer;

enum urpc_channel_type{
    URPC_CHAN_MASTER=0,   //For core 0
    URPC_CHAN_SLAVE=1     //For spawned cores
};

struct urpc_channel{
    struct urpc_buffer buffer_send;
    struct urpc_buffer buffer_rcv;

    struct thread* server_thread;
    bool server_stop_now;
};

errval_t urpc_server_start_listen(struct urpc_channel* channel, bool new_thread);
errval_t urpc_server_stop(struct urpc_channel* channel);
int urpc_server_event_loop(void* data);

errval_t urpc_channel_init(struct urpc_channel* channel, void* fullbuffer, size_t length,
        enum urpc_channel_type);

#endif
