#include <stdio.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/urpc/server.h>
#include <aos/urpc/default_opcodes.h>
#include <omap44xx_map.h>
#include <netutil/user_serial.h>
#include <aos/inthandler.h>
#include "slip_parser.h"

struct aos_rpc *init_rpc;
struct slip_state slip_state;

void serial_input(uint8_t *buf, size_t len){
    slip_raw_rcv(&slip_state, buf, len);
}

static
void icmp_handler(uint32_t from, uint32_t to, uint8_t *buf, size_t len){
    debug_printf("Received icmp message\n");
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

    uint8_t icmp_buffer[20];
    ERR_CHECK("Register ICMP", slip_register_protocol_handler(&slip_state, 0x01, icmp_buffer, sizeof(icmp_buffer),icmp_handler));

    aos_rpc_accept(get_init_rpc());

	return 0;
}
