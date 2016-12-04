#ifndef _ICMP_PARSER_
#define _ICMP_PARSER_

#include <aos/aos.h>

struct __attribute__((packed)) icmp_packet{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence_number;
    uint8_t data[0];
};

#endif //_ICMP_PARSER_
