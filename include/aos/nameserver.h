#ifndef USR_NAMESERVER_NAMESERVER_H_
#define USR_NAMESERVER_NAMESERVER_H_


/**
 * \brief Contacts an RPC endpoint that knows about the namesever (init) and
 * indirectly requests a binding from it
 *
 * \param ret_rpc this rpc will be bound to the nameserver
 */
errval_t nameserver_rpc_init(struct aos_rpc *ret_rpc);

/**
 * \brief
 * \param rpc  the rpc channel
 */
errval_t aos_rpc_nameserver_lookup(struct aos_rpc *rpc);

/**
 * \brief
 * \param rpc  the rpc channel
 */
errval_t aos_rpc_nameserver_enumerate(struct aos_rpc *rpc);

/**
 * \brief
 * \param rpc  the rpc channel
 */
errval_t aos_rpc_nameserver_register(struct aos_rpc *rpc, struct capref ep_cap);

/**
 * \brief
 * \param rpc  the rpc channel
 */
errval_t aos_rpc_nameserver_deregister(struct aos_rpc *rpc);

#endif /* USR_NAMESERVER_NAMESERVER_H_ */
