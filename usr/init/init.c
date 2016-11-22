#include <aos/waitset.h>
#include <aos/morecore.h>
#include <aos/paging.h>
#include <aos/aos_rpc.h>
#include <mm/mm.h>
#include <spawn/spawn.h>

#include "coreboot.h"
#include "processmgr.h"
#include "mem_alloc.h"
#include "lrpc_server.h"
#include "init.h"
#include "urpc/server.h"
#include "urpc/urpc.h"
#include "urpc/opcodes.h"

static struct urpc_channel urpc_chan; // URPC thread holds reference to this object!

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

    // 2. Get boot info. Get it from args or read it from URPC
    bi = argc > 1 ? (struct bootinfo*)strtol(argv[1], NULL, 10) : NULL;
    //void* urpc_read_buffer = NULL;
    void* urpc_buffer= NULL;
    size_t urpc_buffer_size=0;
    if (!bi) {
        assert(my_core_id > 0);
        //TODO: Find a way to replace hardcoded value
        bi = malloc(sizeof(struct bootinfo)+(sizeof(struct mem_region)*6));
        assert(bi);
        memset(bi, 0, sizeof(struct bootinfo)+(sizeof(struct mem_region)*6));

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

        err = coreboot_read_bootinfo_from_urpc(urpc_buffer, &bi, &available_ram, 1);
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
        err = coreboot_urpc_read_bootinfo_modules(urpc_buffer, bi, 1);
        if (err_is_fail(err))
        {
            DEBUG_ERR(err, "read_modules");
            return err;
        }
        coreboot_finished_init(urpc_buffer);
        urpc_channel_init(&urpc_chan, urpc_buffer, urpc_buffer_size, URPC_CHAN_SLAVE);

        void* response=NULL;
        debug_printf("Sending request to first core\n");
        size_t bytes=0;
        err = urpc_client_send(&urpc_chan.buffer_send, URPC_OP_PRINT, "milan", sizeof("milan"), &response, &bytes);
        if (err_is_fail(err))
        {
            DEBUG_ERR(err, "client_send");
            return err;
        }
        debug_printf("Received answer %s bytes: %lu\n", response, bytes);
    }

    // 5. Init RPC server
    aos_rpc_init(&rpc, NULL_CAP, false);
    lmp_server_init(&rpc);
    processmgr_init(&rpc, argv[0]);

    // 6. Boot second core if needed
    if (my_core_id==0){
        debug_printf("--- Starting new core!\n");

        coreboot_init(bi, &urpc_buffer, &urpc_buffer_size);
        ERROR_RET1(urpc_channel_init(&urpc_chan, urpc_buffer, urpc_buffer_size, URPC_CHAN_MASTER));
        ERROR_RET1(urpc_server_start_listen(&urpc_chan, true));
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
    aos_rpc_accept(&rpc);
    urpc_server_stop(&urpc_chan);
    return SYS_ERR_OK;
}
