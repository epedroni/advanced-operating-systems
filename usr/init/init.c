#include <aos/waitset.h>
#include <aos/debug.h>
#include <aos/morecore.h>
#include <aos/paging.h>
#include <aos/aos_rpc.h>
#include <mm/mm.h>
#include <spawn/spawn.h>

#include "coreboot.h"
#include "process/processmgr.h"
#include "mem_alloc.h"
#include "lrpc_server.h"
#include "init.h"
#include <aos/urpc/server.h>
#include <aos/urpc/urpc.h>
#include "urpc/opcodes.h"
#include "urpc/handlers.h"
#include "binding_server.h"

errval_t os_core_initialize(int argc, char** argv)
{
    errval_t err;

    /// /sbin/init initialization sequence.
    // Warning: order of steps MATTERS

    struct coreboot_available_ram_info available_ram;

    // 1. Find core ID
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);
    debug_printf("main() being invoked\n"); // After set_core_id to properly display core id

    ERROR_RET1(cap_retype(cap_selfep, cap_dispatcher, 0,
        ObjType_EndPoint, 0, 1));

    // fill in the nameserver cap with a dummy cap until the nameserver is up - this is how we know if the nameserver is up or not!
    ERROR_RET1(cap_copy(cap_nameserverep, cap_selfep));

    // 2. Get boot info. Get it from args or read it from URPC
    bi = argc > 1 ? (struct bootinfo*)strtol(argv[1], NULL, 10) : NULL;
    //void* urpc_read_buffer = NULL;
    void* urpc_buffer= NULL;
    size_t urpc_buffer_size=0;
    if (!bi) {
        assert(my_core_id > 0);
        //TODO: Find a way to replace hardcoded value
        bi = malloc(sizeof(struct bootinfo)+(sizeof(struct mem_region)*NUM_BOOTINFO_REGIONS));
        assert(bi);
        memset(bi, 0, sizeof(struct bootinfo)+(sizeof(struct mem_region)*NUM_BOOTINFO_REGIONS));

        //TODO: Read this from arguments
        struct frame_identity urpc_frame_id;
        frame_identify(cap_urpc, &urpc_frame_id);
        err = paging_map_frame(get_current_paging_state(), &urpc_buffer, urpc_frame_id.bytes, cap_urpc,
                    NULL, NULL);
        urpc_buffer_size=urpc_frame_id.bytes;
        if (err_is_fail(err))
        {
            DEBUG_ERR(err, "paging_map_frame");
            return err;
        }

        err = coreboot_read_bootinfo_from_urpc(urpc_buffer, &bi, &available_ram);
        if (err_is_fail(err))
        {
            DEBUG_ERR(err, "read_from_urpc");
            return err;
        }
    }

    // 3. Initialize RAM alloc. Requires a correct boot info regions
    assert(bi);
    assert(bi->regions_length);
    err = initialize_ram_alloc(my_core_id, available_ram.ram_base_address, available_ram.ram_size);
    if (err_is_fail(err))
    {
        DEBUG_ERR(err, "initialize_ram_alloc");
        return err;
    }

    // 4. Get module regions caps.
    // Requires RAM alloc initiated, to allocate a L2 CNode.
    if (urpc_buffer)
    {
        err = coreboot_urpc_read_bootinfo_modules(urpc_buffer, bi);
        if (err_is_fail(err))
        {
            DEBUG_ERR(err, "read_modules");
            return err;
        }
        coreboot_finished_init(urpc_buffer);
        ERROR_RET1(urpc_channel_init(&urpc_chan, urpc_buffer, urpc_buffer_size, URPC_CHAN_SLAVE, URPC_OP_COUNT));
    }

    // 5. Init RPC server
    ERROR_RET1(aos_rpc_init(&core_rpc, NULL_CAP, false));
    ERROR_RET1(lmp_server_init(&core_rpc));

    // 6. Boot second core if needed
    if (my_core_id==0){
        coreboot_init(bi, &urpc_buffer, &urpc_buffer_size);
        ERROR_RET1(urpc_channel_init(&urpc_chan, urpc_buffer, urpc_buffer_size, URPC_CHAN_MASTER, URPC_OP_COUNT));
    }
    ERROR_RET1(processmgr_init(my_core_id, argv[0]));

    // 6. Urpc server stuff
    ERROR_RET1(urpc_register_default_handlers(&urpc_chan));
    ERROR_RET1(urpc_server_start_listen(&urpc_chan, true));
    ERROR_RET1(processmgr_register_urpc_handlers(&urpc_chan));

    ERROR_RET1(binding_server_lmp_init(&core_rpc, &urpc_chan));
    ERROR_RET1(binding_server_register_urpc_handlers(&urpc_chan));

    //TODO: Move test to separate function
    char buffer[50];
    for(int i=0;i<0;++i){
        void* response=NULL;
        debug_printf("Sending request for message: %lu\n",i);
        size_t bytes=0;
        snprintf(buffer, sizeof(buffer), "Sending message: %lu", i);
        err = urpc_client_send(&urpc_chan.buffer_send, URPC_OP_PRINT, buffer, sizeof(buffer), &response, &bytes);
        if (err_is_fail(err))
        {
            DEBUG_ERR(err, "client_send");
            return err;
        }
        debug_printf("Received answer for message: %lu with text %s bytes: %lu\n", i, response, bytes);
    }

    #define LOGO(s) debug_printf("%s\n", s);
    if (my_core_id==0){
        LOGO(",-.----.                                                                                                   ");
        LOGO("\\    /  \\                                                             ,---,.                               ");
        LOGO("|   :    \\                              ,---,                       ,'  .'  \\                              ");
        LOGO("|   |  .\\ :                 ,---,     ,---.'|                     ,---.' .' |                      __  ,-. ");
        LOGO(".   :  |: |             ,-+-. /  |    |   | :                     |   |  |: |                    ,' ,'/ /| ");
        LOGO("|   |   \\ : ,--.--.    ,--.'|'   |    |   | |   ,--.--.           :   :  :  /   ,---.     ,---.  '  | |' | ");
        LOGO("|   : .   //       \\  |   |  ,\"' |  ,--.__| |  /       \\          :   |    ;   /     \\   /     \\ |  |   ,' ");
        LOGO(";   | |`-'.--.  .-. | |   | /  | | /   ,'   | .--.  .-. |         |   :     \\ /    /  | /    /  |'  :  /   ");
        LOGO("|   | ;    \\__\\/: . . |   | |  | |.   '  /  |  \\__\\/: . .         |   |   . |.    ' / |.    ' / ||  | '    ");
        LOGO(":   ' |    ,\" .--.; | |   | |  |/ '   ; |:  |  ,\" .--.; |         '   :  '; |'   ;   /|'   ;   /|;  : |    ");
        LOGO(":   : :   /  /  ,.  | |   | |--'  |   | '/  ' /  /  ,.  |         |   |  | ; '   |  / |'   |  / ||  , ;    ");
        LOGO("|   | :  ;  :   .'   \\|   |/      |   :    :|;  :   .'   \\        |   :   /  |   :    ||   :    | ---'     ");
        LOGO("`---'.|  |  ,     .-./'---'        \\   \\  /  |  ,     .-./        |   | ,'    \\   \\  /  \\   \\  /           ");
        LOGO("  `---`   `--`---'                  `----'    `--`---'            `----'       `----'    `----'            ");
        LOGO("                                        ... Well actually we are simply TeamF. But we are still awesome ;)");
    }

    // END OF INIT INITIALIZATION SEQUENCE
    // DONT BREAK THE ORDER OF THE CODE BEFORE, UNLESS YOU KNOW WHAT YOU ARE DOING
    return SYS_ERR_OK;
}

errval_t os_core_events_loop(void)
{
    debug_printf("Entering accept loop forever\n");
    aos_rpc_accept(&core_rpc);
    urpc_server_stop(&urpc_chan);
    return SYS_ERR_OK;
}
