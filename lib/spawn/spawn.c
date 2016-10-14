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

    // 3- Setup childs cspace
    struct cnoderef cnoderef;
    struct capref child_l1_cnode;
    struct cnoderef child_l2_cnodes[ROOTCN_SLOTS_USER];
    err = cnode_create_l1(&child_l1_cnode, &cnoderef);
    if (err_is_fail(err))
        return err_push(err, SPAWN_ERR_SETUP_CSPACE);

    for (int i = 0; i < ROOTCN_SLOTS_USER; ++i)
    {
        err = cnode_create_foreign_l2(child_l1_cnode,
                i,
                &child_l2_cnodes[i]);
        if (err_is_fail(err))
            return err_push(err, SPAWN_ERR_SETUP_CSPACE);
    }

    // Fill capabilities
    // ROOTCN_SLOT_TASKCN
        // TASKCN_SLOT_SELFEP -- done in 6.
        // TASKCN_SLOT_DISPATCHER -- done in 6.
        // TASKCN_SLOT_ROOTCN
        struct capref child_rootcn;
        child_rootcn.cnode = child_l2_cnodes[ROOTCN_SLOT_TASKCN];
        child_rootcn.slot = TASKCN_SLOT_ROOTCN;
        err = cap_copy(child_rootcn, child_l1_cnode);
        if (err_is_fail(err))
            return err_push(err, SPAWN_ERR_SETUP_CSPACE);
        // TASKCN_SLOT_DISPFRAME ??
        // TASKCN_SLOT_ARGSPAGE ??
    // ROOTCN_SLOT_BASE_PAGE_CN
    // Slot for a cnode of BASE_PAGE_SIZE frames
    // TODO: Used?
    struct capref child_frame_ref;
    child_frame_ref.cnode = child_l2_cnodes[ROOTCN_SLOT_BASE_PAGE_CN];
    for (child_frame_ref.slot = 0; child_frame_ref.slot < L2_CNODE_SLOTS; ++child_frame_ref.slot)
    {
        struct capref page_ref;
        err = ram_alloc(&page_ref, BASE_PAGE_SIZE);
        if (err_is_fail(err))
            return err_push(err, SPAWN_ERR_CREATE_PAGECN);
        //TODO: RAM or Frame?
        err = cap_copy(child_frame_ref, page_ref);
        if (err_is_fail(err))
            return err_push(err, SPAWN_ERR_CREATE_PAGECN);
    }
    // ROOTCN_SLOT_PAGECN
    // Slot 0 contains L1 PageTable

    // 4- Setup childs vspace
        // TODO: Implement me
    // 5- Load the ELF binary
    // 6- Setup dispatcher
        // Fill ROOTCN_SLOT_TASKCN.TASKCN_SLOT_SELFEP
        // and ROOTCN_SLOT_TASKCN.TASKCN_SLOT_DISPATCHER
    // 7- Setup environment
    // 8- Make dispatcher runnable

    return SYS_ERR_OK;
}
