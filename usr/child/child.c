
#include <stdio.h>
#include <aos/aos.h>
#include <aos/urpc/udp.h>
#include <aos/urpc/server.h>

struct aos_rpc *init_rpc;

struct udp_state udp_state;

errval_t communicate(void);
errval_t communicate(void){
    init_rpc = get_init_rpc();
    errval_t err;

    debug_printf("Connecting to server socket on different core\n");

    struct capref shared_frame;
    slot_alloc(&shared_frame);
    err=aos_connect_to_port(init_rpc, 42, &shared_frame);
    if(err_is_fail(err)){
        debug_printf("We have a problem in connecting to server socket\n");
    }
//
//    struct capref shared_buffer;
//    size_t ret_bytes;
//    ERROR_RET1(frame_alloc(&shared_buffer, BASE_PAGE_SIZE, &ret_bytes));
//    void* address=NULL;
//    ERROR_RET1(paging_map_frame_attr(get_current_paging_state(),&address,BASE_PAGE_SIZE,
//            shared_buffer,VREGION_FLAGS_READ_WRITE, NULL, NULL));
//    debug_printf("Memseting all to 0\n");
//    memset(address, 0, BASE_PAGE_SIZE);
//
//    err=aos_rpc_create_server_socket(get_init_rpc(), shared_buffer, 42);
//    if(err_is_fail(err)){
//        debug_printf("Failed to send bind\n");
//    }
//
//    struct urpc_channel urpc_chan;
//    urpc_channel_init(&urpc_chan, address, BASE_PAGE_SIZE, URPC_CHAN_MASTER, DEF_URPC_OP_COUNT);
//
//    debug_printf("Starting to listen\n");
//
//    urpc_server_register_handler(&urpc_chan, DEF_URPC_OP_PRINT, handle_print, NULL);
//
//    ERROR_RET1(urpc_server_start_listen(&urpc_chan, false));

    return SYS_ERR_OK;
}

void handle_udp_packet(struct udp_socket socket, uint32_t from, struct udp_packet* data, size_t len);
void handle_udp_packet(struct udp_socket socket, uint32_t from, struct udp_packet* data, size_t len){
    debug_printf("Received UDP packet of length: %lu\n", data->length);
    data->data[data->length-1]=0;
    debug_printf("Received UDP text: %s\n", data->data);
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

	ERR_CHECK("Create udp server",udp_create_server(&udp_state, 12345, handle_udp_packet));

	return 0;
}
