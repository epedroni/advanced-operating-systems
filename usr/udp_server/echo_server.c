#include <stdio.h>
#include <aos/aos.h>
#include <aos/urpc/udp.h>
#include <netutil/htons.h>

struct aos_rpc *init_rpc;
struct udp_state udp_state;

#define RESPONSE_BUFF_SIZE 100
static char response[RESPONSE_BUFF_SIZE];
static char default_resp_prefix[]="response: ";
void* write_address;
size_t max_size;
size_t prefix_size;

static
void handle_udp_packet(struct udp_socket socket, uint32_t from, struct udp_packet* packet, size_t len){
    debug_printf("Received UDP packet!\n");
    size_t data_size=len-sizeof(struct udp_packet);
    strncpy(write_address, (void*)packet->data, data_size);
    udp_send_data(&socket, response, prefix_size+data_size);
}

int main(int argc, char *argv[])
{
    uint16_t my_port=1234;
    prefix_size=sizeof(default_resp_prefix);
    memcpy(response, default_resp_prefix, prefix_size);
    if(argc>1){
        my_port=atoi(argv[1]);
    }
    if(argc>2){
        prefix_size=strlen(argv[2]);
        memcpy(response, argv[2], prefix_size);
    }
    write_address=response+prefix_size;
    max_size=RESPONSE_BUFF_SIZE-prefix_size;
    init_rpc = get_init_rpc();

    debug_printf("Starting server on port: %lu\n", my_port);
    ERR_CHECK("Creating UDP server", udp_create_server(&udp_state, htons(my_port), handle_udp_packet));
    return 0;
}
