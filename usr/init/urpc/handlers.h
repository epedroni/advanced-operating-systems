#ifndef _HEADER_INIT_URPC_HANDLERS
#define _HEADER_INIT_URPC_HANDLERS

#include "server.h"

//struct urpc_message
//{
//    uint32_t opcode;
//    uint32_t length;
//    void* data;
//};
//
//typedef errval_t (*urpc_callback_func_t)(struct urpc_buffer*, struct urpc_message*);
//
//void urpc_server_register_callbacks(urpc_callback_func_t* callbacks_table);

errval_t urpc_register_default_handlers(struct urpc_channel* channel);

#endif