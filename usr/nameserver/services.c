/**
 * \file
 * \brief Name server application
 */

/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */

#include <services.h>

struct registered_service *services = NULL;

errval_t register_service(char *name, struct aos_rpc *rpc) {
    struct registered_service *new_service = malloc(sizeof(struct registered_service));
    new_service->name = name;
    new_service->rpc = rpc;
    new_service->prev = NULL;

    if (services) {
        services->prev = new_service;
        new_service->next = services;
        services = new_service;
    } else {
        services = new_service;
        new_service->next = NULL;
    }

    return SYS_ERR_OK;
}

errval_t deregister_service(char *name) {
    struct registered_service *service = services;
    while (service) {
        if (!strcmp(service->name, name)) {
            if (service->prev) {
                service->prev->next = service->next;
            }
            if (service->next) {
                service->next->prev = service->prev;
            }
            if (service == services) {
                services = service->next;
            }
            return SYS_ERR_OK;
        } else {
            service = service->next;
        }
    }
    return SYS_ERR_OK;
}

errval_t lookup(char *query, struct aos_rpc **ret_rpc) {
    struct registered_service *service = services;
    while (service) {
        if (!strcmp(service->name, query)) {
            debug_printf("Found service: %s\n", service->name);
            *ret_rpc = service->rpc;
            break;
        } else {
            debug_printf("Wrong service: %s, next!\n", service->name);
            service = service->next;
        }
    }
    return SYS_ERR_OK;
}

errval_t enumerate( char *query, size_t *num, char **result) {
    struct registered_service *service = services;
        while (service) {
            debug_printf("Service: %s\n", service->name);
            service = service->next;
        }
    return SYS_ERR_OK;
}
