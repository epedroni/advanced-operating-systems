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

errval_t nameserver_lookup(char *name, struct aos_rpc *ret_rpc) {
    // TODO error handling
    struct capref rpc_cap;
    ERROR_RET1(aos_rpc_nameserver_lookup(get_nameserver_rpc(), name, &rpc_cap));
    return aos_rpc_init(ret_rpc, rpc_cap, true);
}

errval_t nameserver_enumerate(size_t *num, char ***result) {
    return aos_rpc_nameserver_enumerate(get_nameserver_rpc(), num, result);
}

errval_t nameserver_register(char *name, struct aos_rpc *rpc) {
    // TODO error handling
    struct aos_rpc_session* ns_sess = NULL;
    aos_server_add_client(rpc, &ns_sess);
    aos_server_register_client(rpc, ns_sess);

    return aos_rpc_nameserver_register(get_nameserver_rpc(), ns_sess->lc.local_cap, name);
}

errval_t nameserver_deregister(char *name) {
    return aos_rpc_nameserver_deregister(get_nameserver_rpc(), name);
}
