#ifndef _INIT_LRPC_SERVER_H_
#define _INIT_LRPC_SERVER_H_

#include <stdio.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>

errval_t lmp_server_init(struct aos_rpc* rpc);

#endif /* _INIT_LRPC_SERVER_H_ */
