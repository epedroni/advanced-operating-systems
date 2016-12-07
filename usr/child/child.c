
#include <stdio.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/urpc/server.h>

struct aos_rpc *init_rpc;

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

struct __attribute__((packed)) udp_packet{
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
    uint8_t data[0];
};

errval_t handle_print(struct urpc_buffer* buf, struct urpc_message* msg, void* context);
errval_t handle_print(struct urpc_buffer* buf, struct urpc_message* msg, void* context)
{
    debug_printf("Handling received message from other process\n");

    struct udp_packet* packet=(struct udp_packet*)msg->data;
    debug_printf("Received udp length:   %s\n",  packet->data);

    return SYS_ERR_OK;
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

	struct urpc_channel urpc_chan;
    struct capref urpc_cap;
	void* urpc_buffer=NULL;
    size_t urpc_size;
    ERR_CHECK("Creating frame", frame_alloc(&urpc_cap, BASE_PAGE_SIZE, &urpc_size));
    ERR_CHECK("Mapping urpc frame", paging_map_frame(get_current_paging_state(), &urpc_buffer, urpc_size, urpc_cap,
            NULL, NULL));
    ERR_CHECK("urpc channel init", urpc_channel_init(&urpc_chan, urpc_buffer, urpc_size, URPC_CHAN_MASTER, 8));
    ERR_CHECK("urpc server register", urpc_server_register_handler(&urpc_chan, 1, handle_print, NULL));

    ERR_CHECK("Udp connect", aos_rpc_udp_connect(init_rpc, urpc_cap, 100, 100));
	debug_printf("### Starting URPC server\n");
    ERR_CHECK("URPC server start", urpc_server_start_listen(&urpc_chan, false));

    aos_rpc_accept(init_rpc);
	//communicate();

	return 0;
}
