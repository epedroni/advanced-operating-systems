#ifndef USR_NAMESERVER_SERVICES_H_
#define USR_NAMESERVER_SERVICES_H_

#include <stdio.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>

struct registered_service {
    char* name;
    // some sort of endpoint?

    struct registered_service *next, *prev;
};

/**
* @brief initialises an RPC with nameserver
*
* @param rpc the rpc struct to initialise
*
* @return SYS_ERR_OK on success
* errval on failure
*/
errval_t nameserver_rpc_init(struct aos_rpc *rpc);

/**
* @brief register a name binding
*
* @param name the name under which to register the service
* other parameters: the service endpoint itself
*
* @return SYS_ERR_OK on success
* errval on failure
*/
errval_t register_service(char *name);

/**
* @brief deregister a name binding
*
* @param name the name under which the service was registered
*
* @return SYS_ERR_OK on success
* errval on failure
*/
errval_t deregister_service(char *name);


/**
* @brief lookup a name binding
*
* @param query the query string to look up
* other parameters: the returned service endpoint itself
*
* @return SYS_ERR_OK on success
* errval on failure
*/
errval_t lookup(char *query);

/**
* @brief lookup a name binding
*
* @param query the query string to look up
* @param num returns the number of names returned
* @param result array of <num> names returned
*
* @return SYS_ERR_OK on success
* errval on failure
*/
errval_t enumerate(char *query, size_t *num, char **result);

#endif /* USR_NAMESERVER_SERVICES_H_ */
