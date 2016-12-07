#ifndef _ICMP_PARSER_
#define _ICMP_PARSER_

#include <aos/aos.h>
#include "slip_parser.h"

#define ICMP_PROTOCOL_NUMBER 0x01
#define ICMP_BUFF_SIZE 1024

struct __attribute__((packed)) icmp_packet{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence_number;
    uint8_t data[0];
};

struct icmp_state{
    struct slip_state* slip_state;
    uint8_t buffer[ICMP_BUFF_SIZE];
};

errval_t icmp_init(struct icmp_state* icmp_state, struct slip_state* slip_state);

#endif //_ICMP_PARSER_
