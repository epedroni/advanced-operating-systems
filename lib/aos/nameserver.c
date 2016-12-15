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

errval_t nameserver_lookup(struct aos_rpc *rpc, char *name, struct capref *ret_cap) {
    return aos_rpc_nameserver_lookup(rpc, name, ret_cap);
}

errval_t nameserver_enumerate(struct aos_rpc *rpc) {
    return SYS_ERR_OK;
}

errval_t nameserver_register(struct aos_rpc *rpc, struct capref ep_cap, char *name) {
    return aos_rpc_nameserver_register(rpc, ep_cap, name);
}

errval_t nameserver_deregister(struct aos_rpc *rpc) {
    return SYS_ERR_OK;
}
