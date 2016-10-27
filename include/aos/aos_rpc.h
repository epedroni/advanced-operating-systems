/**
 * \file
 * \brief Interface declaration for AOS rpc-like messaging
 */

/*
 * Copyright (c) 2013, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _LIB_BARRELFISH_AOS_MESSAGES_H
#define _LIB_BARRELFISH_AOS_MESSAGES_H

#include <aos/aos.h>

// Maximum 255 opcodes
enum message_opcodes {
    RPC_NULL_OPCODE     = 0,
    RPC_HANDSHAKE,
    RPC_RAM_CAP,
    RPC_NUMBER,
    RPC_STRING,
    RPC_PUT_CHAR,
    RPC_GET_CHAR,
    RPC_SPAWN,
    RPC_GET_NAME,
    RPC_GET_PID,
    RPC_NUM_OPCODES,
};

enum message_flags {
    RPG_FLAG_NONE       = 0x0,
    RPC_FLAG_ACK        = 0x1,
    RPC_FLAG_INCOMPLETE = 0x2,
    RPC_FLAG_ERROR      = 0x4,
};

#define RPC_OPCODE_BITS 8
#define MAKE_RPC_MSG_HEADER(op, flags) (op | (flags << RPC_OPCODE_BITS))
#define RPC_HEADER_OPCODE(header) (header & ((1 << RPC_OPCODE_BITS) - 1))
#define RPC_HEADER_FLAGS(header) (header >> RPC_OPCODE_BITS)

void (*rpc_num_handler_cb)(uintptr_t number);
void (*rpc_string_handler_cb)(char* string);
void (*rpc_get_ram_handler_cb)(size_t bytes, struct capref *retcap, size_t *ret_bytes);
void (*rpc_get_char_handler_cb)(char *retc);
void (*rpc_put_char_handler_cb)(char c);

struct aos_rpc {
    struct lmp_chan lc;
    bool can_send;
    bool ack_received;
    struct waitset* ws;

};

struct number_handler_closure {
    void (*num_handler_cb)(uintptr_t number);
    void *arg;
};

errval_t aos_rpc_register_number_handler(struct aos_rpc *chan,
        struct number_handler_closure cb);

/**
 * \brief send a number over the given channel
 */
errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val);

/**
 * \brief send a string over the given channel
 */
errval_t aos_rpc_send_string(struct aos_rpc *chan, const char *string);

/**
 * \brief request a RAM capability with >= request_bits of size over the given
 * channel.
 */
errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t bytes,
                             struct capref *retcap, size_t *ret_bytes);

/**
 * \brief get one character from the serial port
 */
errval_t aos_rpc_serial_getchar(struct aos_rpc *chan, char *retc);

/**
 * \brief send one character to the serial port
 */
errval_t aos_rpc_serial_putchar(struct aos_rpc *chan, char c);

/**
 * \brief Request process manager to start a new process
 * \arg name the name of the process that needs to be spawned (without a
 *           path prefix)
 * \arg newpid the process id of the newly spawned process
 */
errval_t aos_rpc_process_spawn(struct aos_rpc *chan, char *name,
                               coreid_t core, domainid_t *newpid);

/**
 * \brief Get name of process with id pid.
 * \arg pid the process id to lookup
 * \arg name A null-terminated character array with the name of the process
 * that is allocated by the rpc implementation. Freeing is the caller's
 * responsibility.
 */
errval_t aos_rpc_process_get_name(struct aos_rpc *chan, domainid_t pid,
                                  char **name);

/**
 * \brief Get process ids of all running processes
 * \arg pids An array containing the process ids of all currently active
 * processes. Will be allocated by the rpc implementation. Freeing is the
 * caller's  responsibility.
 * \arg pid_count The number of entries in `pids' if the call was successful
 */
errval_t aos_rpc_process_get_all_pids(struct aos_rpc *chan,
                                      domainid_t **pids, size_t *pid_count);


/**
 * \brief Initialize given rpc channel.
 * TODO: you may want to change the inteface of your init function, depending
 * on how you design your message passing code.
 */
errval_t aos_rpc_init(struct aos_rpc *rpc);

#endif // _LIB_BARRELFISH_AOS_MESSAGES_H
