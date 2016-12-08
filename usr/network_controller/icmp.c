#include "icmp.h"

static
void message_handler(uint32_t from, uint32_t to, uint8_t *buf, size_t len, void* context){
    struct icmp_state* icmp_state=(struct icmp_state*)context;
    debug_printf("Received message from address: 0x%04x\n", from);
    debug_printf("My address is: 0x%04x\n", to);
    struct icmp_packet* packet=(struct icmp_packet*)buf;
    debug_printf("Received ICMP packet. Code: %d type: %d\n", packet->code, packet->type);
    packet->code=0;
    packet->type=0;
    packet->checksum=0;
    packet->checksum=inet_checksum(packet, len);
    slip_send_datagram(icmp_state->slip_state, from, to, 0x01, buf, len);
}

errval_t icmp_init(struct icmp_state* icmp_state, struct slip_state* slip_state){
    icmp_state->slip_state=slip_state;
    ERROR_RET1(slip_register_protocol_handler(slip_state,ICMP_PROTOCOL_NUMBER, icmp_state->buffer, ICMP_BUFF_SIZE, message_handler, icmp_state));

    return SLIP_ERR_OK;
}
