/**
 * \file
 * \brief Implementation of AOS rpc-like messaging
 */

/*
 * Copyright (c) 2013 - 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <aos/aos_rpc.h>

/*
*   // does the sender want to yield their timeslice on success?
	bool sync = flags & LMP_FLAG_SYNC;
	// does the sender want to yield to the target
	// if undeliverable?
	bool yield = flags & LMP_FLAG_YIELD;
	// is the cap (if present) to be deleted on send?
	bool give_away = flags & LMP_FLAG_GIVEAWAY;
 */

static
void cb_recv_ready(void* args){
    struct aos_rpc *rpc=args;
    rpc->ack_received=true;
}

static
void cb_send_ready(void* args){
    struct aos_rpc *rpc=args;
    rpc->can_send=true;
}

static
void wait_for_send(struct aos_rpc* rpc){
    //struct waitset *default_ws = get_default_waitset();
    while (!rpc->can_send) {
        err = event_dispatch(rpc->ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
    rpc->can_send=false;
}

static
void wait_for_ack(struct aos_rpc* rpc){
    while (!rpc->ack_received) {
        err = event_dispatch(rpc->ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
    rpc->ack_received=false;
}

errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val)
{
    // TODO: implement functionality to send a number over the channel
    // given channel and wait until the ack gets returned.

    wait_for_send(chan);
    //send
    wait_for_ack(chan);
    return SYS_ERR_OK;
}

errval_t aos_rpc_send_string(struct aos_rpc *chan, const char *string)
{
    // TODO: implement functionality to send a string over the given channel
    // and wait for a response.

	// we can only send strings of up to 8 characters
	if (sizeof(string) / sizeof(char) > 8) {
		// ??
		return SYS_ERR_LMP_CHAN_SEND;
	}

	errval_t err=lmp_chan_send1(chan->lc->remote_cap, LMP_FLAG_SYNC, NULL_CAP, val);
	if(err_is_fail(err)) {
		DEBUG_ERR(err, "sending number");
	}

    return SYS_ERR_OK;
}

errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t request_bits,
                             struct capref *retcap, size_t *ret_bits)
{
    // TODO: implement functionality to request a RAM capability over the
    // given channel and wait until it is delivered.
    return SYS_ERR_OK;
}

errval_t aos_rpc_serial_getchar(struct aos_rpc *chan, char *retc)
{
    // TODO implement functionality to request a character from
    // the serial driver.
    return SYS_ERR_OK;
}


errval_t aos_rpc_serial_putchar(struct aos_rpc *chan, char c)
{
    // TODO implement functionality to send a character to the
    // serial port.
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_spawn(struct aos_rpc *chan, char *name,
                               coreid_t core, domainid_t *newpid)
{
    // TODO (milestone 5): implement spawn new process rpc
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_name(struct aos_rpc *chan, domainid_t pid,
                                  char **name)
{
    // TODO (milestone 5): implement name lookup for process given a process
    // id
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_all_pids(struct aos_rpc *chan,
                                      domainid_t **pids, size_t *pid_count)
{
    // TODO (milestone 5): implement process id discovery
    return SYS_ERR_OK;
}


errval_t aos_rpc_init(struct aos_rpc *rpc)
{

    ERROR_RET1(lmp_chan_accept(rpc->lc,
            DEFAULT_LMP_BUF_WORDS, cap_initep));
    rpc->ack_received=false;
    rpc->can_send=false;

    /* set receive handler */
    lmp_chan_alloc_recv_slot(rpc->lc);
    struct event_closure rcv_closure={
        .handler=cb_recv_ready,
        .arg=(void*)rpc
    };
    ERROR_RET1(lmp_chan_register_recv(rpc->lc,
            rpc->ws, rcv_closure));

    /* TODO: send local ep to init */
    struct event_closure send_closure={
        .handler=cb_send_ready,
        .arg=(void*)rpc
    };
    debug_printf("lmp_chan_register_send, invoking!\n");
    ERROR_RET1(lmp_chan_register_send(rpc->lc, rpc->ws, send_closure));
    return SYS_ERR_OK;
}
