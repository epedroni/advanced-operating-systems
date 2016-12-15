#include <aos/urpc/udp.h>


static
errval_t handle_data_received(struct urpc_buffer* buf, struct urpc_message* msg, void* context){
    debug_printf("Udp data received\n");
    struct udp_state* udp_state=(struct udp_state*)context;
    struct udp_command_payload* command=(struct udp_command_payload*)msg->data;
    size_t data_length=msg->length-sizeof(struct udp_command_payload);
    struct udp_socket socket={
        .state=udp_state,
        .socket_id=command->header.socket_id
    };

    struct udp_packet* packet=(struct udp_packet*)command->data;
    udp_state->data_received_handler(socket, 0, packet, data_length);

    return SYS_ERR_OK;
}

static
errval_t handle_connection_established(struct urpc_buffer* buf, struct urpc_message* msg, void* context){
    debug_printf("UDP server, connection established\n");
    return SYS_ERR_OK;
}

static
errval_t handle_connection_terminated(struct urpc_buffer* buf, struct urpc_message* msg, void* context){
    debug_printf("UDP connection terminated\n");
    return SYS_ERR_OK;
}

static
errval_t get_network_rpc(struct aos_rpc* network_rpc){
    debug_printf("Attempting to bind with dummy_service via nameserver\n");
    nameserver_lookup(NS_NETWORKING_NAME, network_rpc);

    debug_printf("Received cap, initialising rpc with dummy_service\n");
    return SYS_ERR_OK;
}

static
errval_t init_urpc(struct udp_state* udp_state){
    size_t urpc_buff_size;
    ERROR_RET1(frame_alloc(&udp_state->urpc_cap, BASE_PAGE_SIZE, &urpc_buff_size));
    ERROR_RET1(paging_map_frame(get_current_paging_state(), &udp_state->urpc_buffer, urpc_buff_size, udp_state->urpc_cap, NULL, NULL));

    debug_printf("Registring all necessery urpc message handlers\n");
    ERROR_RET1(urpc_channel_init(&udp_state->urpc_chan, udp_state->urpc_buffer, urpc_buff_size, URPC_CHAN_MASTER, UDP_COMMADN_COUNT));
    ERROR_RET1(urpc_server_register_handler(&udp_state->urpc_chan, UDP_DATAGRAM_RECEIVED, handle_data_received, udp_state));
    ERROR_RET1(urpc_server_register_handler(&udp_state->urpc_chan, UDP_SOCKET_CREATED, handle_connection_established, udp_state));
    ERROR_RET1(urpc_server_register_handler(&udp_state->urpc_chan, UDP_CONNECTION_TERMINATED, handle_connection_terminated, udp_state));

    return SYS_ERR_OK;
}

errval_t udp_create_server(struct udp_state* udp_state, uint16_t port, udp_packet_received_handler data_received){
    ERROR_RET1(init_urpc(udp_state));
    udp_state->data_received_handler=data_received;
    struct aos_rpc network_rpc;
    ERROR_RET1(get_network_rpc(&network_rpc));
    ERROR_RET1(aos_rpc_udp_create_server(&network_rpc, udp_state->urpc_cap, port));
    debug_printf("### Starting UDP server\n");
    ERROR_RET1(urpc_server_start_listen(&udp_state->urpc_chan, false));
    return SYS_ERR_OK;
}

errval_t udp_connect_to_server(struct udp_state* udp_state, uint32_t address, uint16_t port,
        udp_packet_received_handler data_received, udp_connection_created connection_created){

    ERROR_RET1(init_urpc(udp_state));
    struct aos_rpc network_rpc;
    ERROR_RET1(get_network_rpc(&network_rpc));
    udp_state->data_received_handler=data_received;
    ERROR_RET1(aos_rpc_udp_connect(&network_rpc, udp_state->urpc_cap, address, port));
    debug_printf("### Connecting to UDP server\n");
    uint32_t socket_id=0;
    size_t ret_size;
    urpc_client_send_receive_fixed_size(&udp_state->urpc_chan.buffer_send, UDP_GET_CLIENT_SOCKET_ID,
    NULL, 0, &socket_id ,sizeof(uint32_t),&ret_size);
    debug_printf("Received socket id: %lu\n", socket_id);
    struct udp_socket created_socket={
            .socket_id=socket_id,
            .state=udp_state
    };
    connection_created(created_socket);
    ERROR_RET1(urpc_server_start_listen(&udp_state->urpc_chan, false));
    return SYS_ERR_OK;
}

errval_t udp_send_data(struct udp_socket* socket, void* data, size_t len){
    struct udp_command_payload response;
    size_t return_size=0;

    struct udp_command_payload_header command_header={
            .socket_id=socket->socket_id
    };
    ERROR_RET1(urpc_client_send_chunck(&socket->state->urpc_chan.buffer_send, &command_header, sizeof(struct udp_command_payload_header), true));
    ERROR_RET1(urpc_client_send_final_chunck_receive_fixed_size(&socket->state->urpc_chan.buffer_send, UDP_SEND_DATAGRAM,
                data, len,
                &response, sizeof(struct udp_command_payload), &return_size));

    debug_printf("Server received data\n");
    return SYS_ERR_OK;
}
