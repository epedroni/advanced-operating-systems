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

errval_t os_core_initialize(int argc, char** argv)
{
    errval_t err;

    /// /sbin/init initialization sequence.
    // Warning: order of steps MATTERS

    // 1. Find core ID
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);
    debug_printf("main() being invoked\n"); // After set_core_id to properly display core id
    ERROR_RET1(cap_retype(cap_selfep, cap_dispatcher, 0,
        ObjType_EndPoint, 0, 1));

    // 2. Get boot info. Get it from args or read it from URPC
    bi = argc > 1 ? (struct bootinfo*)strtol(argv[1], NULL, 10) : NULL;
    void* urpc_read_buffer = NULL;
    if (!bi) {
        assert(my_core_id > 0);
        bi = malloc(sizeof(struct bootinfo)+(sizeof(struct mem_region)*2));
        assert(bi);
        memset(bi, 0, sizeof(struct bootinfo)+(sizeof(struct mem_region)*2));

        //TODO: Read this from arguments
        struct frame_identity urpc_frame_id;
        frame_identify(cap_urpc, &urpc_frame_id);
        err = paging_map_frame(get_current_paging_state(), &urpc_read_buffer, urpc_frame_id.bytes, cap_urpc,
                    NULL, NULL);
        if (err_is_fail(err))
        {
            DEBUG_ERR(err, "paging_map_frame");
            return err;
        }

        err = coreboot_read_bootinfo_from_urpc(urpc_read_buffer, &bi, 1);
        if (err_is_fail(err))
        {
            DEBUG_ERR(err, "read_from_urpc");
            return err;
        }
    }


    // 3. Initialize RAM alloc. Requires a correct boot info regions
    assert(bi);
    assert(bi->regions_length);
    err = initialize_ram_alloc(my_core_id);
    if (err_is_fail(err))
    {
        DEBUG_ERR(err, "initialize_ram_alloc");
        return err;
    }

    // 4. Get module regions caps.
    // Requires RAM alloc initiated, to allocate a L2 CNode.
    if (urpc_read_buffer)
    {
        err = coreboot_urpc_read_bootinfo_modules(urpc_read_buffer, bi, 1);
        if (err_is_fail(err))
        {
            DEBUG_ERR(err, "read_modules");
            return err;
        }
    }

    // 5. Init RPC server
    aos_rpc_init(&rpc, NULL_CAP, false);
    lmp_server_init(&rpc);
    processmgr_init(&rpc, argv[0]);

    // 6. Boot second core if needed
    if (my_core_id==0){
        debug_printf("--- Starting new core!\n");
        coreboot_init(bi);
    }

    #define LOGO(s) debug_printf("%s\n", s);
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
    // END OF INIT INITIALIZATION SEQUENCE
    // DONT BREAK THE ORDER OF THE CODE BEFORE, UNLESS YOU KNOW WHAT YOU ARE DOING
    return SYS_ERR_OK;
}

errval_t os_core_events_loop(void)
{
    debug_printf("Entering accept loop forever\n");
    aos_rpc_accept(&rpc);
    return SYS_ERR_OK;
}
