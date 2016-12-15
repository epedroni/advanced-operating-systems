#ifndef INCLUDE_AOS_NAMESERVER_H_
#define INCLUDE_AOS_NAMESERVER_H_

errval_t nameserver_lookup(char *name, struct aos_rpc *ret_rpc);

errval_t nameserver_enumerate(void);

errval_t nameserver_register(char *name, struct capref ep_cap);

errval_t nameserver_deregister(void);

#endif /* INCLUDE_AOS_NAMESERVER_H_ */
