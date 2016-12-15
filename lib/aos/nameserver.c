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

errval_t nameserver_lookup(char *name, struct capref *ret_cap) {
    return aos_rpc_nameserver_lookup(get_nameserver_rpc(), name, ret_cap);
}

errval_t nameserver_enumerate(void) {
    return SYS_ERR_OK;
}

errval_t nameserver_register(char *name, struct capref ep_cap) {
    return aos_rpc_nameserver_register(get_nameserver_rpc(), ep_cap, name);
}

errval_t nameserver_deregister(void) {
    return SYS_ERR_OK;
}
