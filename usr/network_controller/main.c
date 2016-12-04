#include <stdio.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/urpc/server.h>
#include <aos/urpc/default_opcodes.h>
#include <omap44xx_map.h>
#include <netutil/user_serial.h>
#include <aos/inthandler.h>
#include "slip_parser.h"
#include <netutil/htons.h>
#include <netutil/checksum.h>

struct aos_rpc *init_rpc;
struct slip_state slip_state;

void serial_input(uint8_t *buf, size_t len){
    slip_raw_rcv(&slip_state, buf, len);
}

struct __attribute__((packed)) icmp_packet{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence_number;
    uint8_t data[0];
};

static
void print_ip(uint32_t ip)
{
    unsigned char bytes[4];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;
    debug_printf("IP = %d.%d.%d.%d\n", bytes[3], bytes[2], bytes[1], bytes[0]);
}

static
void icmp_handler(uint32_t from, uint32_t to, uint8_t *buf, size_t len){
    debug_printf("Received icmp message, dumping it\n");
    print_ip(lwip_ntohl(from));
    print_ip(lwip_ntohl(to));
    struct icmp_packet* packet=(struct icmp_packet*)buf;
    debug_printf("Type %lu code: %lu data:\n", (packet->type), (packet->code));
    for(int i=0;i<len-sizeof(struct icmp_packet);++i){
        printf("%02x ", packet->data[i]);
    }
    printf("\nSending reply!\n");

    packet->code=0;
    packet->type=0;
    packet->checksum=0;
    packet->checksum=inet_checksum(buf, len);
    slip_send_datagram(&slip_state, from, to, 0x01, buf, len);

}

int main(int argc, char *argv[])
{
    init_rpc = get_init_rpc();
    ERR_CHECK("Initializing IRQ cap", aos_rpc_get_special_capability(init_rpc, AOS_CAP_IRQ, &cap_irq));

    struct capref uart_frame;
    slot_alloc(&uart_frame);
    ERR_CHECK("Getting UART4 frame", aos_rpc_get_special_capability(init_rpc, AOS_CAP_NETWORK_UART, &uart_frame));
    void* uart_address=NULL;
    ERR_CHECK("mapping uart frame", paging_map_frame_attr(get_current_paging_state(), &uart_address,    //TODO: Send uart frame size!
            OMAP44XX_MAP_L4_PER_UART4_SIZE, uart_frame, VREGION_FLAGS_READ_WRITE | VREGION_FLAGS_NOCACHE, NULL, NULL));

    ERR_CHECK("Init serial", serial_init((lvaddr_t)uart_address, UART4_IRQ));
    ERR_CHECK("Initializing SLIP parser", slip_init(&slip_state, serial_write));

    uint8_t icmp_buffer[23];
    ERR_CHECK("Register ICMP", slip_register_protocol_handler(&slip_state, 0x01, icmp_buffer, sizeof(icmp_buffer),icmp_handler));

    aos_rpc_accept(get_init_rpc());

	return 0;
}
