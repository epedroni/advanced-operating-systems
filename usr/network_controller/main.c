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

//Data buffer variables
#define UART_RCV_BUFFER_SIZE 2000
uint8_t uart_receive_buffer[UART_RCV_BUFFER_SIZE];
volatile size_t buffer_start;
volatile size_t buffer_end;

#define DEBUG_BUFF_SIZE 800
size_t debug_buff_index;
uint8_t debug_buffer[DEBUG_BUFF_SIZE];

void serial_input(uint8_t *buf, size_t len){
    assert(len==1);
    for(size_t i=0; i<len; ++i){

        size_t end=(buffer_end+1)%UART_RCV_BUFFER_SIZE;

        while(end==buffer_start){
            debug_printf("------ PROCESSING IS TOOOOO SLOW\n");
            thread_yield();
        }
//
//        debug_buffer[debug_buff_index++]=buf[i];
//        debug_buff_index=debug_buff_index%DEBUG_BUFF_SIZE;
//
//        if(debug_buff_index==0){
//            debug_printf("Printing out buffer backtrace!\n");
//            for(int j=0;j<DEBUG_BUFF_SIZE;++j){
//                printf("%02x ", debug_buffer[j]);
//            }
//            printf("\n");
//        }

        uart_receive_buffer[buffer_end]=buf[i];
        buffer_end=end;
    }
}

int serial_buffer_consumer(void* args);
int serial_buffer_consumer(void* args){
    debug_printf("Started consumer thread!\n");
    while(1){

        while(buffer_start==buffer_end){
            thread_yield();
        }

        slip_consume_byte(&slip_state, uart_receive_buffer[buffer_start]);
        size_t start=(buffer_start+1)%UART_RCV_BUFFER_SIZE;
        buffer_start=start;
    }

    return 0;
}

struct __attribute__((packed)) icmp_packet{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence_number;
    uint8_t data[0];
};

struct __attribute__((packed)) udp_packet{
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
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
//    debug_printf("Received icmp message, dumping it\n");
    print_ip(lwip_ntohl(from));
    print_ip(lwip_ntohl(to));
    struct icmp_packet* packet=(struct icmp_packet*)buf;
//    debug_printf("Type %lu code: %lu data:\n", (packet->type), (packet->code));

    thread_yield();

//    for(int i=0;i<len-sizeof(struct icmp_packet);++i){
//        debug_printf("%02x ", packet->data[i]);
//    }
//    printf("\nSending reply!\n");

    packet->code=0;
    packet->type=0;
    packet->checksum=0;
    packet->checksum=inet_checksum(buf, len);
    slip_send_datagram(&slip_state, from, to, 0x01, buf, len);
}

static
void udp_handler(uint32_t from, uint32_t to, uint8_t *buf, size_t len){
    debug_printf("Received UDP packet\n");
    struct udp_packet* packet=(struct udp_packet*)buf;
    debug_printf("Received UDP message: %s length: %lu\n", packet->data, len);

    uint16_t tmp=packet->source_port;
    packet->source_port=packet->dest_port;
    packet->dest_port=tmp;
    packet->checksum=0x0;

    debug_printf("UDP packet printout\n");
    for(int i=0;i<len;++i){
//        printf("%02x ", buf[i]);
    }
    printf("\n");

    slip_send_datagram(&slip_state, from, to, 0x11, buf, len);
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

    buffer_start=0;
    buffer_end=0;

    // Starting thread
    struct thread* consumer_thread=thread_create(serial_buffer_consumer, NULL);
    if(consumer_thread==NULL){
        debug_printf("Couldn't create thread!!");
    }

    ERR_CHECK("Init serial", serial_init((lvaddr_t)uart_address, UART4_IRQ));
    ERR_CHECK("Initializing SLIP parser", slip_init(&slip_state, serial_write));

    uint8_t icmp_buffer[1100];
    ERR_CHECK("Register ICMP", slip_register_protocol_handler(&slip_state, 0x01, icmp_buffer, sizeof(icmp_buffer), icmp_handler));
    uint8_t udp_buffer[64];
    ERR_CHECK("Register UDP", slip_register_protocol_handler(&slip_state, 0x11, udp_buffer, sizeof(udp_buffer), udp_handler));

    while (true) {
        errval_t err=event_dispatch(get_default_waitset());
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

	return 0;
}
