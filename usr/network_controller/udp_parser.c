#include "udp_parser.h"

static
struct udp_remote_connection* find_remote_connection(struct udp_local_connection* local_connection, uint32_t remote_address, uint16_t remote_port){
    struct udp_remote_connection* rc;
    for(rc=local_connection->remote_connection_head;rc!=NULL;rc=rc->next){
        if(rc->remote_port==remote_port && rc->remote_address==remote_address){
            return rc;
        }
    }
    return NULL;
}

static
struct udp_remote_connection* find_socket_by_id(struct udp_local_connection* local_connection, uint32_t socket_id){
    struct udp_remote_connection* rc;
    for(rc=local_connection->remote_connection_head;rc!=NULL;rc=rc->next){
        if(rc->socket.socket_id==socket_id){
            return rc;
        }
    }
    return NULL;
}

static
errval_t get_udp_socket_id(struct urpc_buffer* urpc, struct urpc_message* msg, void* context){
    struct udp_local_connection* local_connection=(struct udp_local_connection*)context;
    assert(local_connection);
    uint32_t* socket_id= msg->data;
    assert(local_connection->remote_connection_head);
    *socket_id=local_connection->remote_connection_head->socket.socket_id;
    urpc_server_answer(urpc, socket_id, sizeof(uint32_t));

    return SYS_ERR_OK;
}

static
errval_t send_udp_datagram(struct urpc_buffer* urpc, struct urpc_message* msg, void* context){
    struct udp_local_connection* local_connection=(struct udp_local_connection*)context;
    debug_printf("##### Sending UDP datagram, packet size: %lu\n", msg->length);

    struct udp_command_payload* cmd=(struct udp_command_payload*)msg->data;
    uint32_t socket_id=cmd->header.socket_id;
    struct udp_remote_connection* remote_connection=find_socket_by_id(local_connection, socket_id);
    assert(remote_connection && "Unknown socket id");
    debug_printf("Remote connection address: 0x%04x\n", remote_connection);

    size_t payload_size=msg->length-sizeof(struct udp_command_payload_header);
    struct udp_packet* send_packet=(struct udp_packet*)malloc(payload_size+sizeof(struct udp_packet));
    debug_printf("Payload size: %lu\n", payload_size);
    send_packet->checksum=0;
    send_packet->length=lwip_htons(payload_size+sizeof(struct udp_packet));
    send_packet->source_port=local_connection->local_port;
    send_packet->dest_port=remote_connection->remote_port;
    assert(remote_connection->remote_address);
    memcpy(send_packet->data, cmd->data, payload_size);

    slip_send_datagram(local_connection->udp_parser_state->slip_state,
            remote_connection->remote_address, local_connection->udp_parser_state->slip_state->my_ip_address,
            UDP_PROTOCOL_NUMBER, (void*)send_packet, payload_size+sizeof(struct udp_packet));

    free(send_packet);

    return SYS_ERR_OK;
}

static
errval_t forward_datagram(struct udp_local_connection* local_open_connection, uint32_t socket_id, void* buf, size_t len){
    struct udp_command_payload response;
    size_t return_size=0;

    struct udp_command_payload_header command_header={
        .socket_id=socket_id
    };

    ERROR_RET1(urpc_client_send_chunck(&local_open_connection->udp_state.urpc_chan.buffer_send, &command_header,
            sizeof(struct udp_command_payload_header), true));
    ERROR_RET1(urpc_client_send_final_chunck_receive_fixed_size(&local_open_connection->udp_state.urpc_chan.buffer_send, UDP_DATAGRAM_RECEIVED,
                buf, len,
                &response, sizeof(struct udp_command_payload), &return_size));

    return SYS_ERR_OK;
}

static
void udp_handle_connection_to_server(struct udp_parser_state* udp_state, struct udp_local_connection* local_open_connection,
        uint32_t from, uint32_t to, void* buf, size_t len){
    debug_printf("Handling connection to server, local_open_connection: 0x%04x\n", local_open_connection);
    struct udp_packet* udp_packet=(struct udp_packet*)buf;
    struct udp_remote_connection* remote_connection=find_remote_connection(local_open_connection, from, udp_packet->source_port);
    if(remote_connection==NULL){
        debug_printf("We don't have opened remote connection, opening!\n");
        remote_connection=(struct udp_remote_connection*)malloc(sizeof(struct udp_remote_connection));
        remote_connection->next=local_open_connection->remote_connection_head;
        remote_connection->remote_address=from;
        remote_connection->socket.socket_id=local_open_connection->last_socket_id++;
        remote_connection->remote_port=udp_packet->source_port;
        local_open_connection->remote_connection_head=remote_connection;
        debug_printf("Created new remote connection address 0x%04x\n", remote_connection);
    }

    forward_datagram(local_open_connection, remote_connection->socket.socket_id, buf, len);
}

static
void udp_handle_connection_to_client(struct udp_parser_state* udp_state, struct udp_local_connection* local_open_connection,
        uint32_t from, uint32_t to, void* buf, size_t len){

    debug_printf("Handling connection to client, local_open_connection: 0x%04x\n", local_open_connection);
    struct udp_packet* udp_packet=(struct udp_packet*)buf;
    struct udp_remote_connection* remote_connection=find_remote_connection(local_open_connection, from, udp_packet->source_port);
    if(remote_connection==NULL){
        debug_printf("We don't have opened remote connection for client, ignoring packet!\n");
        return;
    }

    forward_datagram(local_open_connection, remote_connection->socket.socket_id, buf, len);
}

static
void udp_data_handler(uint32_t from, uint32_t to, uint8_t *buf, size_t len, void* context){

    struct udp_parser_state* udp_state=(struct udp_parser_state*)context;
    struct udp_local_connection* local_open_connection;
    struct udp_packet* udp_packet=(struct udp_packet*)buf;
    for(local_open_connection=udp_state->local_connectoin_head;local_open_connection!=NULL;local_open_connection=local_open_connection->next){
        debug_printf("Found application with port %lu!\n", ntohs(local_open_connection->local_port));
        if(udp_packet->dest_port!=local_open_connection->local_port){
            continue;
        }
        if(local_open_connection->connection_type==UDP_PARSER_CONNECTION_SERVER){
            udp_handle_connection_to_server(udp_state, local_open_connection, from, to, buf, len);
        }else{  // We have a response to client connection
            udp_handle_connection_to_client(udp_state, local_open_connection, from, to, buf, len);
        }
        return;
    }
    debug_printf("Sending request to unknown port, ignoring datagram\n");
    //TODO: Send ICMP error message, or don't send anything??
}

static
errval_t udp_create_local_connection(struct capref urpc_cap, struct udp_local_connection* local_connection){
    debug_printf("init urpc related stuff\n");
    ERROR_RET1(paging_map_frame(get_current_paging_state(), &local_connection->udp_state.urpc_buffer, BASE_PAGE_SIZE, urpc_cap,
            NULL, NULL));

    ERROR_RET1(urpc_channel_init(&local_connection->udp_state.urpc_chan, local_connection->udp_state.urpc_buffer, BASE_PAGE_SIZE,
            URPC_CHAN_SLAVE, UDP_COMMADN_COUNT));
    ERROR_RET1(urpc_server_register_handler(&local_connection->udp_state.urpc_chan, UDP_SEND_DATAGRAM, send_udp_datagram, local_connection));
    ERROR_RET1(urpc_server_register_handler(&local_connection->udp_state.urpc_chan, UDP_GET_CLIENT_SOCKET_ID, get_udp_socket_id, local_connection));
    debug_printf("----- creating new thread to listen for messages ---\n");
    ERROR_RET1(urpc_server_start_listen(&local_connection->udp_state.urpc_chan, true));
    return SYS_ERR_OK;
}

errval_t udp_create_server_connection(struct udp_parser_state* udp_state, struct capref urpc_cap, uint16_t port){
    debug_printf("Creating UDP server\n");
    struct udp_local_connection* local_connection=(struct udp_local_connection*)malloc(sizeof(struct udp_local_connection));
    debug_printf("Creating port: %lu\n", port);

    local_connection->connection_type=UDP_PARSER_CONNECTION_SERVER;
    local_connection->last_socket_id=0;
    local_connection->local_port=port;
    local_connection->next=udp_state->local_connectoin_head;
    udp_state->local_connectoin_head=local_connection;
    local_connection->udp_parser_state=udp_state;
    local_connection->remote_connection_head=NULL;

    udp_create_local_connection(urpc_cap, local_connection);
    return SYS_ERR_OK;
}

errval_t udp_create_client_connection(struct udp_parser_state* udp_state, struct capref urpc_cap, uint32_t address, uint16_t port){
    debug_printf("Creating UDP client\n");
    struct udp_local_connection* local_connection=(struct udp_local_connection*)malloc(sizeof(struct udp_local_connection));
    //Create local connection
    debug_printf("Setting all stuff up\n");
    local_connection->connection_type=UDP_PARSER_CONNECTION_CLIENT;
    local_connection->last_socket_id=0;
    local_connection->local_port=htons(udp_state->first_available_port++);
    local_connection->next=udp_state->local_connectoin_head;
    udp_state->local_connectoin_head=local_connection;
    local_connection->udp_parser_state=udp_state;
    local_connection->remote_connection_head=NULL;
    udp_create_local_connection(urpc_cap, local_connection);
    // Create remote connection
    struct udp_remote_connection* remote_connection=(struct udp_remote_connection*)malloc(sizeof(struct udp_remote_connection));
    remote_connection->next=local_connection->remote_connection_head;
    remote_connection->remote_address=address;
    remote_connection->socket.socket_id=local_connection->last_socket_id++;
    remote_connection->remote_port=port;
    local_connection->remote_connection_head=remote_connection;
    debug_printf("Created new remote connection address 0x%04x\n", remote_connection);

    return SYS_ERR_OK;
}

errval_t udp_init(struct udp_parser_state* udp_state, struct slip_state* slip_state){
    udp_state->first_available_port=50000;  //TODO: Implement mechanism for tracking last used port
    udp_state->slip_state=slip_state;
    udp_state->local_connectoin_head=NULL;
    ERROR_RET1(slip_register_protocol_handler(slip_state, UDP_PROTOCOL_NUMBER, udp_state->data, UDP_BUFF_SIZE, udp_data_handler, udp_state));

    return SYS_ERR_OK;
}
