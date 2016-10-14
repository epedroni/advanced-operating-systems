#include <aos/aos.h>
#include <spawn/spawn.h>

#include <elf/elf.h>
#include <aos/dispatcher_arch.h>
#include <barrelfish_kpi/paging_arm_v7.h>
#include <barrelfish_kpi/domain_params.h>
#include <spawn/multiboot.h>

extern struct bootinfo *bi;
errval_t elf_allocator(void *state, genvaddr_t base, size_t size, uint32_t flags, void **ret);

// TODO(M2): Implement this function such that it starts a new process
// TODO(M4): Build and pass a messaging channel to your child process
errval_t spawn_load_by_name(void * binary_name, struct spawninfo * si) {
    printf("spawn start_child: starting: %s\n", binary_name);

    // 1- Get the binary from multiboot image
    struct mem_region* process_mem_reg=multiboot_find_module(bi,binary_name);

    if (!process_mem_reg)
        return SPAWN_ERR_FIND_MODULE;

    memset(si, 0, sizeof(*si));
    si->binary_name = binary_name;

    debug_printf("Received address of mem_region: 0x%X\n",process_mem_reg);

    // Binary frame
    struct capref spawned_process_frame={
    		.cnode=cnode_module,
			.slot=process_mem_reg->mrmod_slot
    };
    struct frame_identity spawned_process_frame_id;
    ERROR_RET2(frame_identify(spawned_process_frame, &spawned_process_frame_id),
            SPAWN_ERR_MAP_MODULE);

    // 2- Map multiboot module in your address space
    struct paging_state* page_state=get_current_paging_state();
    void* address=NULL;
	ERROR_RET2(paging_map_frame_attr(page_state, &address, BASE_PAGE_SIZE,
			spawned_process_frame, VREGION_FLAGS_READ_WRITE, NULL, NULL),
            SPAWN_ERR_MAP_MODULE);

    char* elf = (char*)address;
    debug_printf("Beginning: 0x%x %c %c %c. Size=%u\n",
        elf[0], elf[1], elf[2], elf[3],
        spawned_process_frame_id.bytes);

    // 3- Setup childs cspace
    struct cnoderef cnoderef;
    struct capref child_l1_cnode;
    struct cnoderef child_l2_cnodes[ROOTCN_SLOTS_USER];
    ERROR_RET2(cnode_create_l1(&child_l1_cnode, &cnoderef), SPAWN_ERR_SETUP_CSPACE);

    for (int i = 0; i < ROOTCN_SLOTS_USER; ++i)
    {
        ERROR_RET2(cnode_create_foreign_l2(child_l1_cnode,
                i, &child_l2_cnodes[i]), SPAWN_ERR_SETUP_CSPACE);
    }

    // Fill capabilities
    // ROOTCN_SLOT_TASKCN
        // TASKCN_SLOT_SELFEP -- done in 6.
        // TASKCN_SLOT_DISPATCHER -- done in 6.
        // TASKCN_SLOT_ROOTCN
        struct capref child_rootcn;
        child_rootcn.cnode = child_l2_cnodes[ROOTCN_SLOT_TASKCN];
        child_rootcn.slot = TASKCN_SLOT_ROOTCN;
        ERROR_RET2(cap_copy(child_rootcn, child_l1_cnode),
            SPAWN_ERR_MINT_ROOTCN);
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
        ERROR_RET2(ram_alloc(&page_ref, BASE_PAGE_SIZE), SPAWN_ERR_CREATE_SMALLCN);
        //TODO: RAM or Frame?
        ERROR_RET2(cap_copy(child_frame_ref, page_ref), SPAWN_ERR_CREATE_SMALLCN);
    }
    // ROOTCN_SLOT_PAGECN
    // Slot 0 contains L1 PageTable

    // 4- Setup childs vspace
        // TODO: Implement me
    // 5- Load the ELF binary
    genvaddr_t child_entry_point;
    ERROR_RET2(elf_load(EM_ARM, elf_allocator,
        NULL, (lvaddr_t)address,
        spawned_process_frame_id.bytes, &child_entry_point),
        SPAWN_ERR_LOAD);
    struct Elf32_Shdr* got = elf32_find_section_header_name((lvaddr_t)address,
        spawned_process_frame_id. bytes, ".got");
    if (!got)
        return SPAWN_ERR_LOAD;

    // 6- Setup dispatcher
        // Fill ROOTCN_SLOT_TASKCN.TASKCN_SLOT_SELFEP
        // and ROOTCN_SLOT_TASKCN.TASKCN_SLOT_DISPATCHER
    // 7- Setup environment
    // 8- Make dispatcher runnable

    return SYS_ERR_OK;
}

errval_t elf_allocator(void *state, genvaddr_t base, size_t size, uint32_t flags, void **ret)
{
    struct capref cap;
    ERROR_RET1(ram_alloc(&cap, size));
    // TODO
    return SYS_ERR_OK;
}
