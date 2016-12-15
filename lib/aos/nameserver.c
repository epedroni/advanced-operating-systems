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
#include <aos/nameserver.h>

errval_t nameserver_rpc_init(struct aos_rpc *ret_rpc) {
    return aos_rpc_bind_to_nameserver(get_init_rpc(), ret_rpc);
}

// This only works with init RPC!
static errval_t aos_rpc_bind_to_nameserver(struct aos_rpc *rpc_init, struct aos_rpc *ret_rpc)
{
    if (!rpc_init->server_sess)
        return RPC_ERR_INVALID_ARGUMENTS;

    debug_printf("Sending request to init\n");
    // Request nameserver endpoint
    ERROR_RET1(wait_for_send(rpc_init->server_sess));
    ERROR_RET1(lmp_chan_send1(&rpc_init->server_sess->lc,
        LMP_FLAG_SYNC,
        NULL_CAP,
        RPC_NAMESERVER_LOOKUP));

    debug_printf("Waiting for init response\n");
    struct lmp_recv_msg message=LMP_RECV_MSG_INIT;
    struct capref received_ep;

    ERROR_RET1(recv_block(rpc_init->server_sess,
        &message,
        &received_ep));

    debug_printf("------------------------------------------------------- Response received, initialising rpc\n");
    struct capability cap;
    debug_cap_identify(received_ep, &cap);
    debug_printf("Local cap type: 0x%x\n", cap.type);
    debug_printf("\tListener: 0x%x\n", cap.u.endpoint.listener);
    debug_printf("\tOffset: 0x%x\n", cap.u.endpoint.epoffset);

    return aos_rpc_init(ret_rpc, received_ep, true, false);
}

errval_t aos_rpc_nameserver_lookup(struct aos_rpc *rpc)
{
    return SYS_ERR_OK;
}

errval_t aos_rpc_nameserver_enumerate(struct aos_rpc *rpc)
{
    return SYS_ERR_OK;
}

errval_t aos_rpc_nameserver_register(struct aos_rpc *rpc, struct capref ep_cap) {
    return SYS_ERR_OK;
}

errval_t aos_rpc_nameserver_deregister(struct aos_rpc *rpc)
{
    return SYS_ERR_OK;
}
