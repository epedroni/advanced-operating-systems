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
    debug_printf("we are ready to receive\n");
    struct aos_rpc *rpc=args;

    struct lmp_recv_msg message = LMP_RECV_MSG_INIT;
    struct capref dummy_capref;

    lmp_chan_recv(&rpc->lc, &message, &dummy_capref);

    if(message.words[0] & RPC_ACK){
        rpc->ack_received=true;
        debug_printf("We received ack, yay!\n");
    }
}

static
void cb_send_ready(void* args){
    debug_printf("** we can send!\n");
    struct aos_rpc *rpc=args;
    rpc->can_send=true;
}

static
void wait_for_send(struct aos_rpc* rpc){
    debug_printf("waiting for send\n");
    errval_t err;

    lmp_chan_register_send(&rpc->lc, rpc->ws, MKCLOSURE(cb_send_ready, (void*)rpc));

    while (!rpc->can_send) {
        err = event_dispatch(rpc->ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
    rpc->can_send=false;
    debug_printf("can send received\n");
}

static
void wait_for_ack(struct aos_rpc* rpc){
    errval_t err;

    lmp_chan_register_recv(&rpc->lc,
            rpc->ws, MKCLOSURE(cb_recv_ready, (void*)rpc));

    debug_printf("wait for ack\n");
    while (!rpc->ack_received) {
        err = event_dispatch(rpc->ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
    rpc->ack_received=false;

    debug_printf("ack received\n");
}

// Waits for send, sends and waits for ack
#define RPC_CHAN_WRAPPER_SEND(chan, call) \
    { \
        wait_for_send(chan); \
        errval_t _err = call; \
        if (err_is_fail(_err)) \
            DEBUG_ERR(_err, "Send failed in " __FILE__ ":%d", __LINE__); \
        wait_for_ack(chan); \
    }

/*
 * sends a number over the channel
 */
errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val)
{
    RPC_CHAN_WRAPPER_SEND(chan,
        lmp_chan_send2(&chan->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_NUMBER,
            val));
    return SYS_ERR_OK;
}

errval_t aos_rpc_send_string(struct aos_rpc *chan, const char *string)
{
	// we can only send strings of up to 8 characters, need to do the stub
	if (sizeof(string) / sizeof(char) > 8) {
		return LIB_ERR_LMP_CHAN_SEND;
	}

	wait_for_send(chan);
	errval_t err=lmp_chan_send2(&chan->lc, LMP_FLAG_SYNC, NULL_CAP, RPC_STRING, string[0]);
	if(err_is_fail(err)) {
		DEBUG_ERR(err, "sending string");
	}
	wait_for_ack(chan);

    return SYS_ERR_OK;
}

errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t request_bits,
                             struct capref *retcap, size_t *ret_bits)
{
    RPC_CHAN_WRAPPER_SEND(chan,
        lmp_chan_send2(&chan->lc,
            LMP_FLAG_SYNC, NULL_CAP,
            RPC_RAM_CAP,
            request_bits));
	return SYS_ERR_OK;
}

errval_t aos_rpc_serial_getchar(struct aos_rpc *chan, char *retc)
{
    // TODO implement functionality to request a character from
    // the serial driver.

    wait_for_send(chan);
	errval_t err=lmp_chan_send1(&chan->lc, LMP_FLAG_SYNC, NULL_CAP, RPC_GET_CHAR);
	if (err_is_fail(err))
		DEBUG_ERR(err, "sending get char request");

	// TODO: wait for ack, place char in retc

    return SYS_ERR_OK;
}

/*
 * Sends a character to the serial port.
 */
errval_t aos_rpc_serial_putchar(struct aos_rpc *chan, char c)
{
    RPC_CHAN_WRAPPER_SEND(chan,
        lmp_chan_send2(&chan->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_PUT_CHAR,
            c));
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

static
errval_t aos_rpc_send_handshake(struct aos_rpc *chan, struct capref selfep)
{
    RPC_CHAN_WRAPPER_SEND(chan,
        lmp_chan_send1(&chan->lc,
            LMP_FLAG_SYNC,
            selfep,
            RPC_HANDSHAKE));
    return SYS_ERR_OK;
}

errval_t aos_rpc_init(struct aos_rpc *rpc)
{
    debug_printf("aos_rpc_init: invoked\n");
    rpc->ws=get_default_waitset();
    ERROR_RET1(lmp_chan_accept(&rpc->lc,
            DEFAULT_LMP_BUF_WORDS, cap_initep));
    rpc->ack_received=false;
    rpc->can_send=false;

    /* set receive handler */
    lmp_chan_alloc_recv_slot(&rpc->lc);
    debug_printf("lmp_chan_register_send, invoking!\n");
    aos_rpc_send_handshake(rpc, rpc->lc.local_cap);

    // store it at a well known location
    set_init_rpc(rpc);

    return SYS_ERR_OK;
}
