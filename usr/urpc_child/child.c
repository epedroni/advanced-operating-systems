#include <stdio.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/urpc/server.h>
#include <aos/urpc/default_opcodes.h>

struct aos_rpc *init_rpc;

errval_t aos_slab_refill(struct slab_allocator *slabs){
    debug_printf("Aos slab refill!\n");
    //TODO: We have to think of a way how to provide refill function to every application
    return SYS_ERR_OK;
}

static
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
    debug_printf("We are connected!\n");
    struct frame_identity shared_frame_id;
    frame_identify(shared_frame,&shared_frame_id);
    debug_printf("Received frame info: base: [0x%08x] and size: [0x%08x]\n", (int)shared_frame_id.base, (int)shared_frame_id.bytes);
    void* address=NULL;
    ERROR_RET1(paging_map_frame_attr(get_current_paging_state(),&address, BASE_PAGE_SIZE,
            shared_frame,VREGION_FLAGS_READ_WRITE, NULL, NULL));
    struct urpc_channel urpc_chan;
    urpc_channel_init(&urpc_chan, address,BASE_PAGE_SIZE, URPC_CHAN_SLAVE, DEF_URPC_OP_COUNT);
    char text[]="milan";
    size_t ret_size;
    urpc_client_send_receive_fixed_size(&urpc_chan.buffer_send,DEF_URPC_OP_PRINT, text,sizeof(text),text, sizeof(text),&ret_size);
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
    communicate();
    return 0;
}
