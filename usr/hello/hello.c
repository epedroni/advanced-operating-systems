/**
 * \file
 * \brief Hello world application
 */

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
#include <aos/nameserver.h>

errval_t listen(void);

static
errval_t handle_print(struct urpc_buffer* buf, struct urpc_message* msg, void* context)
{
    debug_printf("++++++++++++++ Handling string in hello %s \n", msg->data);

    return SYS_ERR_OK;
}

errval_t listen(void){
    //init_rpc = get_init_rpc();
    errval_t err;

    debug_printf("Creating server socket\n");
    struct capref shared_buffer;
    size_t ret_bytes;
    ERROR_RET1(frame_alloc(&shared_buffer, BASE_PAGE_SIZE, &ret_bytes));

    void* address=NULL;
    ERROR_RET1(paging_map_frame_attr(get_current_paging_state(),&address, BASE_PAGE_SIZE,
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
	debug_printf("Received %d arguments \n",argc);
	for(int i=0;i<argc;++i){
		debug_printf("Printing argument: %d %s\n",i, argv[i]);
	}
	errval_t err;

	debug_printf("init rpc: 0x%x\n", get_init_rpc());
	err = aos_rpc_send_number(get_init_rpc(), (uintptr_t) 42);
	if(err_is_fail(err)){
		DEBUG_ERR(err, "Could not send number");
	}

//	listen();
//	return SYS_ERR_OK;

    err = aos_rpc_send_string(get_init_rpc(), "milan, hello this is dog! :) hahahhahahahahahahahahaha\n");
	if(err_is_fail(err)){
		DEBUG_ERR(err, "Could not send simple string");
	}

	// get all pids, there should be two (init and us)
	domainid_t* pidptr;
	uint32_t pidcount;
	debug_printf("Listing all PIDs...\n");
	err = aos_rpc_process_get_all_pids(get_init_rpc(), &pidptr, &pidcount);
	if(err_is_fail(err)){
		DEBUG_ERR(err, "Could not get PIDs");
		return 0;
	}
	debug_printf("... got %d PIDs\n", pidcount);
	for (int i = 0; i < pidcount; i++)
		debug_printf("Received PID: %d\n", pidptr[i]);
	free(pidptr);

	// spawn memeater
	debug_printf("Spawning memeater via RPC from hello\n");
	domainid_t mem_eater_pid = 42;
	err = aos_rpc_process_spawn(get_init_rpc(), "/armv7/sbin/memeater", 0, &mem_eater_pid);
	if(err_is_fail(err)){
		DEBUG_ERR(err, "Could not spawn memeater");
	}

	// get all pids again, there should be three now
	debug_printf("Getting the name of each running process\n");
	err = aos_rpc_process_get_all_pids(get_init_rpc(), &pidptr, &pidcount);
	if(err_is_fail(err)){
		DEBUG_ERR(err, "Could not get PIDs");
		return 0;
	}
	debug_printf("... got %d PIDs\n", pidcount);

	char* name;
	for (int i = 0; i < pidcount; i++) {
		err = aos_rpc_process_get_name(get_init_rpc(), pidptr[i], &name);
		if(err_is_ok(err))
		{
			debug_printf("PID: %d, name: \"%s\"\n", pidptr[i], name);
			free(name);
		}
		else
			DEBUG_ERR(err, "Could not get domain name [pid=%d]\n", pidptr[i]);
	}
	free(pidptr);

	debug_printf("Trying to get the name of a PID that does not exist\n");
	err = aos_rpc_process_get_name(get_init_rpc(), 1000, &name);
	if(err_is_fail(err)){
		debug_printf("Could not get domain name, that is expected\n");
	}
	else
	{
		debug_printf("FATAL ERROR");
		debug_printf("PID: %d, name: \"%s\"\n", 1000, name);
		return 0;
	}

	debug_printf("Trying to spawn a domain that does not exist\n");
	domainid_t fail_pid;
	err = aos_rpc_process_spawn(get_init_rpc(), "/armv7/sbin/fail", 0, &fail_pid);
	if(err_is_fail(err)){
		debug_printf("Could not spawn domain, that is expected\n");
	}
	debug_printf("Returned PID: %d\n", fail_pid);

	debug_printf("Waiting for memeater [PID=%d] to return\n", (int)mem_eater_pid);
	while (!err_is_fail(aos_rpc_process_get_name(get_init_rpc(), mem_eater_pid, &name)))
		free(name);

	debug_printf("Memeater appears to have returned, check all pids\n");
	err = aos_rpc_process_get_all_pids(get_init_rpc(), &pidptr, &pidcount);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "Could not get PIDs");
		return 0;
    }
	debug_printf("... got %d PIDs\n", pidcount);
    for (int i = 0; i < pidcount; i++) {
        err = aos_rpc_process_get_name(get_init_rpc(), pidptr[i], &name);
        if(err_is_fail(err)){
            DEBUG_ERR(err, "Could not get domain name [pid=%d]\n", pidptr[i]);
			continue;
        }
        debug_printf("PID: %d, name: \"%s\"\n", pidptr[i], name);
		free(name);
    }
	free(pidptr);

	return 0;
}
