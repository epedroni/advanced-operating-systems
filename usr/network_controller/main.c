#include <stdio.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/urpc/server.h>
#include <aos/urpc/default_opcodes.h>
#include <omap44xx_map.h>
#include <netutil/user_serial.h>
#include <aos/inthandler.h>
#include "slip_parser.h"
#include "icmp.h"
#include "udp_parser.h"

struct aos_rpc *init_rpc;
struct slip_state slip_state;
struct icmp_state icmp_state;
struct udp_parser_state udp_state;

//Data buffer variables
#define UART_RCV_BUFFER_SIZE 2000
uint8_t uart_receive_buffer[UART_RCV_BUFFER_SIZE];
volatile size_t buffer_start;
volatile size_t buffer_end;
volatile size_t buffer_write_offset;

struct thread_mutex buffer_lock;
struct thread_cond buffer_content_changed;

#define DEBUG_BUFF_SIZE 3500
#define OFFSET_BUFFER   1133
size_t debug_buff_index;
uint8_t debug_buffer[DEBUG_BUFF_SIZE];

void serial_input(uint8_t *buf, size_t len){
    thread_mutex_lock(&buffer_lock);
    for(size_t i=0; i<len; ++i){

        size_t end=(buffer_end+buffer_write_offset+1)%UART_RCV_BUFFER_SIZE;

        while(end==buffer_start){
            debug_printf("------ CONSUMER IS TOOOOO SLOW\n");
            thread_cond_wait(&buffer_content_changed, &buffer_lock);
        }

        size_t tmp_end=(buffer_end+buffer_write_offset)%UART_RCV_BUFFER_SIZE;
        uart_receive_buffer[tmp_end]=buf[i];
        if(++buffer_write_offset==OFFSET_BUFFER || buf[i]==0xC0){
            buffer_write_offset=0;
            buffer_end=end;
            thread_cond_signal(&buffer_content_changed);
        }
    }
    thread_mutex_unlock(&buffer_lock);
}

int serial_buffer_consumer(void* args);
int serial_buffer_consumer(void* args){
    debug_printf("Started consumer thread!\n");
    while(1){
        thread_mutex_lock(&buffer_lock);
        while(buffer_start==buffer_end){
            thread_cond_wait(&buffer_content_changed, &buffer_lock);
        }
        thread_mutex_unlock(&buffer_lock);

        //Do processing
        ERR_CHECK("Consume byte", slip_consume_byte(&slip_state, uart_receive_buffer[buffer_start]));

        thread_mutex_lock(&buffer_lock);
        size_t start=(buffer_start+1)%UART_RCV_BUFFER_SIZE;
        buffer_start=start;
        thread_cond_signal(&buffer_content_changed);
        thread_mutex_unlock(&buffer_lock);
    }

    return 0;
}

struct urpc_channel urpc_chan;

void cb_accept_loop(void* args);
void cb_accept_loop(void* args){
    debug_printf("Create connection loop invoked!\n");

    struct lmp_recv_msg message = LMP_RECV_MSG_INIT;
    struct capref received_cap=NULL_CAP;
    lmp_chan_recv(&init_rpc->server_sess->lc, &message, &received_cap);

    if(!capcmp(received_cap, NULL_CAP)){
        debug_printf("Capabilities changed, allocating new slot\n");
        lmp_chan_alloc_recv_slot(&init_rpc->server_sess->lc);
    }

    uint32_t connection_type=RPC_HEADER_OPCODE(message.words[0]);
    if(connection_type==UDP_PARSER_CONNECTION_SERVER){
        debug_printf("Creating new server connection\n");
        udp_create_server_connection(&udp_state, received_cap, message.words[1]);
    }else if(connection_type==UDP_PARSER_CONNECTION_CLIENT){
        debug_printf("Creating new client connection\n");
        udp_create_client_connection(&udp_state, received_cap, message.words[1], message.words[2]);
    }else{
        debug_printf("ERROR! unknown connection type %lu\n", message.words[0]);
    }
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
    buffer_write_offset=0;
    thread_cond_init(&buffer_content_changed);
    thread_mutex_init(&buffer_lock);

    // Starting thread
    struct thread* consumer_thread=thread_create(serial_buffer_consumer, NULL);
    if(consumer_thread==NULL){
        debug_printf("Couldn't create thread!!");
    }

    ERR_CHECK("Init serial", serial_init((lvaddr_t)uart_address, UART4_IRQ));
    ERR_CHECK("Initializing SLIP parser", slip_init(&slip_state, serial_write));

    ERR_CHECK("Init ICMP", icmp_init(&icmp_state, &slip_state));
    ERR_CHECK("Init UDP", udp_init(&udp_state, &slip_state));

    domainid_t pid;
    ERR_CHECK("Spawning child process", aos_rpc_process_spawn(init_rpc, "/armv7/sbin/child", 0, &pid));

    // Handle requests
    ERR_CHECK("Register receive", lmp_chan_register_recv(&init_rpc->server_sess->lc, init_rpc->ws, MKCLOSURE(cb_accept_loop, NULL)));

    aos_rpc_accept(init_rpc);
	return 0;
}
