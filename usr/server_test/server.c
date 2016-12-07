/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */

#include <stdio.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/urpc/server.h>
#include <aos/urpc/default_opcodes.h>

struct aos_rpc *init_rpc;

errval_t listen(void);

static
errval_t handle_print(struct urpc_buffer* buf, struct urpc_message* msg, void* context)
{
    debug_printf("Handling string in hello\n");

    return SYS_ERR_OK;
}

errval_t listen(void){
    init_rpc = get_init_rpc();
    errval_t err;

    debug_printf("Creating server socket\n");
    struct capref shared_buffer;
    size_t ret_bytes;
    ERROR_RET1(frame_alloc(&shared_buffer, BASE_PAGE_SIZE, &ret_bytes));

    void* address=NULL;
    ERROR_RET1(paging_map_frame_attr(get_current_paging_state(), &address, BASE_PAGE_SIZE,
            shared_buffer,VREGION_FLAGS_READ_WRITE, NULL, NULL));
    assert(address);
    memset(address, 0, BASE_PAGE_SIZE);

    debug_printf("Allocated frame for sharing data of size: %lu\n", ret_bytes);

    err=aos_rpc_create_server_socket(get_init_rpc(), shared_buffer, 42);
    if(err_is_fail(err)){
        debug_printf("Failed to send bind\n");
    }

    struct urpc_channel urpc_chan;
    urpc_channel_init(&urpc_chan, address, BASE_PAGE_SIZE, URPC_CHAN_MASTER, DEF_URPC_OP_COUNT);

    debug_printf("Starting to listen\n");

    urpc_server_register_handler(&urpc_chan, DEF_URPC_OP_PRINT, handle_print, NULL);

    ERROR_RET1(urpc_server_start_listen(&urpc_chan, false));

    return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
	listen();
	return SYS_ERR_OK;
}
