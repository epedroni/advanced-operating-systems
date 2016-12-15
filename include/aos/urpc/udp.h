#ifndef _LIB_URPC_UDP_
#define _LIB_URPC_UDP_

#include <aos/urpc/server.h>
#include <aos/aos_rpc.h>
#include <aos/nameserver.h>

#define NS_NETWORKING_NAME "networking"

struct __attribute__((packed)) udp_packet{
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
    uint8_t data[0];
};

struct udp_socket{
    struct udp_state* state;
    uint32_t socket_id;
};

typedef void (*udp_packet_received_handler)(struct udp_socket socket, uint32_t from, struct udp_packet* data, size_t len);
typedef void (*udp_connection_created)(struct udp_socket socket);

struct udp_state{
    struct capref urpc_cap;
    struct urpc_channel urpc_chan;
    udp_packet_received_handler data_received_handler;
    void* urpc_buffer;
};

enum udp_connection_type {
    UDP_PARSER_CONNECTION_SERVER,
    UDP_PARSER_CONNECTION_CLIENT
};

enum udp_command_type{
    UDP_SOCKET_CREATED,
    UDP_ERROR,
    UDP_DATAGRAM_RECEIVED,
    UDP_SEND_DATAGRAM,
    UDP_GET_CLIENT_SOCKET_ID,
    UDP_CONNECTION_CLOSE,
    UDP_CONNECTION_TERMINATED,
    UDP_COMMADN_COUNT
};

struct udp_command_payload_header{
    uint32_t socket_id;
};

struct udp_command_payload{
    struct udp_command_payload_header header;
    uint8_t data[];
};

errval_t udp_create_server(struct udp_state* udp_state, struct aos_rpc* nameserver_rpc, uint16_t port, udp_packet_received_handler data_received);
errval_t udp_connect_to_server(struct udp_state* udp_state, struct aos_rpc* nameserver_rpc, uint32_t address, uint16_t port, udp_packet_received_handler data_received, udp_connection_created connection_created);
errval_t udp_send_data(struct udp_socket* socket, void* data, size_t len);

#endif  //_LIB_URPC_UDP_
