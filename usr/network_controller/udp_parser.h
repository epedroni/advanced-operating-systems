#ifndef _UDP_PARSER_
#define _UDP_PARSER_

#include "slip_parser.h"
#include <aos/urpc/udp.h>

#define UDP_PROTOCOL_NUMBER 0x11
#define UDP_BUFF_SIZE 1024

struct udp_parser_state;

struct udp_remote_connection {
    uint16_t remote_port;
    uint32_t remote_address;
    struct udp_socket socket;
    struct udp_remote_connection* next;
};

struct udp_local_connection {
    uint32_t last_socket_id;
    uint16_t local_port;
    enum udp_connection_type connection_type;
    struct udp_state udp_state;
    struct udp_parser_state* udp_parser_state;
    struct udp_remote_connection* remote_connection_head;
    struct udp_local_connection* next;
};

struct udp_parser_state {
    struct slip_state* slip_state;
    struct udp_local_connection* local_connectoin_head;
    uint16_t first_available_port;
    uint8_t data[UDP_BUFF_SIZE];
};

errval_t udp_init(struct udp_parser_state* udp_state, struct slip_state* slip_state);

errval_t udp_create_server_connection(struct udp_parser_state* udp_state, struct capref urpc_cap, uint16_t port);
errval_t udp_create_client_connection(struct udp_parser_state* udp_state, struct capref urpc_cap, uint32_t address, uint16_t port);

#endif //_UDP_PARSER_
