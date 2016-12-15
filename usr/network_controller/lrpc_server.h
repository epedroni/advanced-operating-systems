#ifndef _NETWORK_LRPC_SERVER_H_
#define _NETWORK_LRPC_SERVER_H_

#include <stdio.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>

errval_t lmp_init_networking_services(struct aos_rpc* rpc,
        aos_rpc_handler connect_to_server,
        aos_rpc_handler create_server);

#endif /* _NETWORK_LRPC_SERVER_H_ */
