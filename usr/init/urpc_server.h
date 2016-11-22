#ifndef _HEADER_INIT_URPC_SERVER
#define _HEADER_INIT_URPC_SERVER

#include <aos/aos.h>

struct urpc_buffer;

errval_t urpc_server_start_listen(struct urpc_buffer* buf, bool new_thread);
errval_t urpc_server_stop(void);
int urpc_server_event_loop(void* data);

#endif
