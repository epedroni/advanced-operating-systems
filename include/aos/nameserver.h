#ifndef INCLUDE_AOS_NAMESERVER_H_
#define INCLUDE_AOS_NAMESERVER_H_

errval_t nameserver_lookup(char *name, struct aos_rpc *ret_rpc);

errval_t nameserver_enumerate(size_t *num, char ***result);

errval_t nameserver_register(char *name, struct aos_rpc *rpc);

errval_t nameserver_deregister(char *name);

#endif /* INCLUDE_AOS_NAMESERVER_H_ */
