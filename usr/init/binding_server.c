#include "binding_server.h"
#include "init.h"

static struct binding_server_state binding_state;

static
errval_t create_connection(struct frame_identity* shared_frame, uint32_t port){

    struct open_connection* connection = malloc(sizeof(struct open_connection));
    connection->frame_info.base=shared_frame->base;
    connection->frame_info.bytes=shared_frame->bytes;
    connection->port=port;

    connection->next=binding_state.head;
    binding_state.head=connection;

    return SYS_ERR_OK;
}

static
errval_t find_connection(uint32_t port, struct frame_identity* shared_frame){

    for(struct open_connection* connection=binding_state.head;
            connection!=NULL; connection=connection->next){

        if(connection->port==port){
            shared_frame->base=connection->frame_info.base;
            shared_frame->bytes=connection->frame_info.bytes;
            return SYS_ERR_OK;
        }
    }

    return BINDSRV_ERR_PORT_NOT_FOUND;
}

static
errval_t handle_create_server_socket(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    debug_printf("We are creating new server socket\n");

    uint32_t port=msg->words[1];
    struct frame_identity shared_frame_id;
    ERROR_RET1(frame_identify(received_capref, &shared_frame_id));

    ERROR_RET1(create_connection(&shared_frame_id, port));

    debug_printf("Created socket for port %lu, with frame info: [0x%08x] and size: [0x%08x]\n",
            port, shared_frame_id.base, shared_frame_id.bytes);

    return SYS_ERR_OK;
}

static
errval_t handle_establish_connection(struct urpc_buffer* buf, struct urpc_message* msg, void* context)
{
    debug_printf("Establishing connection\n");
    URPC_CHECK_READ_SIZE(msg, sizeof(uint32_t));
    uint32_t* port=msg->data;

    struct frame_identity frame_info={
            .base=0,
            .bytes=0
    };
    errval_t err=find_connection(*port, &frame_info);
    if(err_is_fail(err)){
        debug_printf("Connection refused\n");
        return err;
    }

    debug_printf("Found connection object, sending frame info: base: [0x%08x] and size: [0x%08x]\n",
            (int)frame_info.base, (int)frame_info.bytes);
    ERROR_RET1(urpc_server_answer(buf, &frame_info, sizeof(struct frame_identity)));

    return SYS_ERR_OK;
}

static
errval_t handle_connect_to_socket(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    debug_printf("Connecting to socket\n");

    struct urpc_channel* channel=context;
    uint32_t pid=msg->words[1];

    struct frame_identity received_frame={
            .base=0,
            .bytes=0
    };
    size_t received_size;
    ERROR_RET1(urpc_client_send_receive_fixed_size(&channel->buffer_send, URPC_OP_CONNECT_TO_SOCKET, &pid, sizeof(pid),
            &received_frame,sizeof(struct frame_identity),&received_size));

    assert(received_size==sizeof(struct frame_identity));

    debug_printf("Received frame info: base: [0x%08x] and size: [0x%08x]\n", (int)received_frame.base, (int)received_frame.bytes);

    //TODO: Forge and return capability
    struct capref return_cap;
    slot_alloc(&return_cap);
    ERROR_RET1(frame_forge(return_cap, received_frame.base, received_frame.bytes, my_core_id))
    ERROR_RET1(lmp_chan_send1(&sess->lc,
        LMP_FLAG_SYNC,
        return_cap,
        MAKE_RPC_MSG_HEADER(RPC_CONNECT_TO_SOCKET, RPC_FLAG_ACK)
    ));

    return SYS_ERR_OK;
}

errval_t binding_server_lmp_init(struct aos_rpc* _rpc, struct urpc_channel* channel){
    binding_state.head=NULL;

    aos_rpc_register_handler(_rpc, RPC_CREATE_SERVER_SOCKET, handle_create_server_socket, true);
    aos_rpc_register_handler_with_context(_rpc, RPC_CONNECT_TO_SOCKET, handle_connect_to_socket, false, channel);
    return SYS_ERR_OK;
}

errval_t binding_server_register_urpc_handlers(struct urpc_channel* channel){
    debug_printf("Registered urpc connection handler\n");
    urpc_server_register_handler(channel, URPC_OP_CONNECT_TO_SOCKET, handle_establish_connection, NULL);
    return SYS_ERR_OK;

}
