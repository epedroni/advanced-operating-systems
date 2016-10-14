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

    // 1- Get the binary from multiboot image
    errval_t err;
    struct mem_region* process_mem_reg=multiboot_find_module(bi,binary_name);

    if (!process_mem_reg)
        return SPAWN_ERR_LOAD;

    memset(si, 0, sizeof(*si));
    si->binary_name = binary_name;

    debug_printf("Received address of mem_region: 0x%X\n",process_mem_reg);

    // Binary frame
    struct capref spawned_process={
    		.cnode=cnode_module,
			.slot=process_mem_reg->mrmod_slot
    };

    // 2- Map multiboot module in your address space
    struct paging_state* page_state=get_current_paging_state();
    void* address=NULL;
	err = paging_map_frame_attr(page_state, &address, BASE_PAGE_SIZE,
			spawned_process, VREGION_FLAGS_READ_WRITE, NULL, NULL);
    if (err_is_fail(err))
        return err_push(err, SPAWN_ERR_SPAN);

    char* elf = (char*)address;
    debug_printf("Beginning: 0x%x %c %c %c\n", elf[0], elf[1], elf[2], elf[3]);
    // TODO: Implement me
    // 3- Setup childs cspace
    // 4- Setup childs vspace
    // 5- Load the ELF binary
    // 6- Setup dispatcher
    // 7- Setup environment
    // 8- Make dispatcher runnable

    return SYS_ERR_OK;
}
