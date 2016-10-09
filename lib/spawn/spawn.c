#include <aos/aos.h>
#include <spawn/spawn.h>

#include <elf/elf.h>
#include <aos/dispatcher_arch.h>
#include <barrelfish_kpi/paging_arm_v7.h>
#include <barrelfish_kpi/domain_params.h>
#include <spawn/multiboot.h>

extern struct bootinfo *bi;

// TODO(M2): Implement this function such that it starts a new process
// TODO(M4): Build and pass a messaging channel to your child process
errval_t spawn_load_by_name(void * binary_name, struct spawninfo * si) {
    printf("spawn start_child: starting: %s\n", binary_name);

    errval_t err;
    struct mem_region* process_mem_reg=multiboot_find_module(bi,binary_name);

    if(!process_mem_reg){
    	debug_printf("Error finding module! Aborting\n");
    	return SYS_ERR_OK;
    }

    // Init spawninfo
    memset(si, 0, sizeof(*si));
    si->binary_name = binary_name;

    debug_printf("Received address of mem_region: 0x%X\n",process_mem_reg);

    struct capref spawned_process={
    		.cnode=cnode_module,
			.slot=process_mem_reg->mrmod_slot
    };

    struct paging_state* page_state=get_current_paging_state();
    void* address=NULL;
	err=paging_map_frame_attr(page_state, &address, BASE_PAGE_SIZE,
			spawned_process, VREGION_FLAGS_READ_WRITE, NULL, NULL);

    // TODO: Implement me
    // - Get the binary from multiboot image
    // - Map multiboot module in your address space
    // - Setup childs cspace
    // - Setup childs vspace
    // - Load the ELF binary
    // - Setup dispatcher
    // - Setup environment
    // - Make dispatcher runnable

    return SYS_ERR_OK;
}
