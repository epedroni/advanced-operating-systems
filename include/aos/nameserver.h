#ifndef INCLUDE_AOS_NAMESERVER_H_
#define INCLUDE_AOS_NAMESERVER_H_

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
errval_t nameserver_lookup(struct aos_rpc *rpc, char *name, struct capref *ret_cap);

/**
 * \brief
 * \param rpc  the rpc channel
 */
errval_t nameserver_enumerate(struct aos_rpc *rpc);

/**
 * \brief Registers a new service on the nameserver
 * \param rpc  the rpc channel
 */
errval_t nameserver_register(struct aos_rpc *rpc, struct capref ep_cap, char *name);

/**
 * \brief
 * \param rpc  the rpc channel
 */
errval_t nameserver_deregister(struct aos_rpc *rpc);

#endif /* INCLUDE_AOS_NAMESERVER_H_ */
