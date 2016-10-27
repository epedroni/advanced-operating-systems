#ifndef _INIT_LRPC_SERVER_H_
#define _INIT_LRPC_SERVER_H_

#include <stdio.h>
#include <aos/aos.h>
#include <spawn/spawn.h>
#include <aos/aos_rpc.h>

struct lmp_server_state{
    size_t buffer_capacity;
    char* buffer;

    size_t current_buff_position;
};

errval_t lmp_server_init(struct aos_rpc* rpc, struct lmp_server_state* lmp_state);

#endif /* _INIT_LRPC_SERVER_H_ */
