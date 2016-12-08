
#include <stdio.h>
#include <aos/aos.h>
#include <aos/urpc/udp.h>
#include <aos/urpc/server.h>
#include <netutil/htons.h>

struct aos_rpc *init_rpc;

struct udp_state udp_state;

void handle_udp_packet(struct udp_socket socket, uint32_t from, struct udp_packet* data, size_t len);
void handle_udp_packet(struct udp_socket socket, uint32_t from, struct udp_packet* data, size_t len){
    debug_printf("Remote porrt: %lu local port: %lu\n", data->source_port, data->dest_port);
    uint16_t data_length=lwip_ntohs(data->length)-sizeof(struct udp_packet);
    data->data[data_length-1]=0;
    debug_printf("Received UDP packet of length: %lu and data lenght: %lu\n", len, data_length);
    debug_printf("Received UDP text: %s\n", data->data);

    udp_send_data(&socket, data->data, data_length);
}

int main(int argc, char *argv[])
{
	debug_printf("Received %d arguments \n",argc);
	for(int i=0;i<argc;++i){
		debug_printf("Printing argument: %d %s\n",i, argv[i]);
	}
	errval_t err;

    init_rpc = get_init_rpc();
	debug_printf("init rpc: 0x%x\n", init_rpc);
	err = aos_rpc_send_number(get_init_rpc(), (uintptr_t)42);
	if(err_is_fail(err)){
		DEBUG_ERR(err, "Could not send number");
	}

	ERR_CHECK("Create udp server", udp_create_server(&udp_state, 12345, handle_udp_packet));

	return 0;
}
