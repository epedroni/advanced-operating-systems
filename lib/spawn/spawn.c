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
    si->binary_name = malloc(strlen(binary_name) + 1);
    strcpy(si->binary_name, binary_name);

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
	ERROR_RET2(paging_map_frame_attr(page_state, &address, spawned_process_frame_id.bytes,
			spawned_process_frame, VREGION_FLAGS_READ_WRITE, NULL, NULL),
            SPAWN_ERR_MAP_MODULE);

    char* elf = (char*)address;
    debug_printf("Beginning at 0x%x: 0x%x %c %c %c. Size=%u\n",
        (int)address, elf[0], elf[1], elf[2], elf[3],
        spawned_process_frame_id.bytes);

    // 3- Setup childs cspace
    struct cnoderef cnoderef;
    ERROR_RET2(cnode_create_l1(&si->l1_cnode_cap, &cnoderef), SPAWN_ERR_SETUP_CSPACE);

    for (int i = 0; i < ROOTCN_SLOTS_USER; ++i)
    {
        ERROR_RET2(cnode_create_foreign_l2(si->l1_cnode_cap,
                i, &si->l2_cnodes[i]), SPAWN_ERR_SETUP_CSPACE);
    }

    // Fill capabilities
    // ROOTCN_SLOT_TASKCN
        // TASKCN_SLOT_SELFEP -- done in 6.
        // TASKCN_SLOT_DISPATCHER -- done in 6.
        // TASKCN_SLOT_ROOTCN
        struct capref child_rootcn;
        child_rootcn.cnode = si->l2_cnodes[ROOTCN_SLOT_TASKCN];
        child_rootcn.slot = TASKCN_SLOT_ROOTCN;
        ERROR_RET2(cap_copy(child_rootcn, si->l1_cnode_cap),
            SPAWN_ERR_MINT_ROOTCN);
        // TASKCN_SLOT_DISPFRAME ??
        // TASKCN_SLOT_ARGSPAGE ??
    // ROOTCN_SLOT_BASE_PAGE_CN
    // Slot for a cnode of BASE_PAGE_SIZE frames
    // TODO: Used?

    struct capref child_frame_ref;
    child_frame_ref.cnode = si->l2_cnodes[ROOTCN_SLOT_BASE_PAGE_CN];
    for (child_frame_ref.slot = 0; child_frame_ref.slot < L2_CNODE_SLOTS; ++child_frame_ref.slot)
    {
        struct capref page_ref;
        ERROR_RET2(ram_alloc(&page_ref, BASE_PAGE_SIZE), SPAWN_ERR_CREATE_SMALLCN);
        //TODO: RAM or Frame?
        ERROR_RET2(cap_copy(child_frame_ref, page_ref), SPAWN_ERR_CREATE_SMALLCN);
    }
    // 4- Setup childs vspace
    // Slot 0 contains L1 PageTable
	struct capref l1_vnode_dest={
		.cnode=si->l2_cnodes[ROOTCN_SLOT_PAGECN],
		.slot=0
	};
	// Allocate L1 arm vnode
	ERROR_RET2(vnode_create(l1_vnode_dest, ObjType_VNode_ARM_l1),
        SPAWN_ERR_L1_VNODE_CREATE);
    debug_printf("Created child L1 pagetable\n");

    // 5- Load the ELF binary
    genvaddr_t child_entry_point;
    debug_printf("Loading ELF binary...\n");
    // TODO: CRASHING. WHY???
    ERROR_RET2(elf_load(EM_ARM, elf_allocator,
        NULL, (lvaddr_t)address,
        spawned_process_frame_id.bytes, &child_entry_point),
        SPAWN_ERR_LOAD);
    debug_printf("elf32_find_section_header_name...\n");
    struct Elf32_Shdr* got = elf32_find_section_header_name((lvaddr_t)address,
        spawned_process_frame_id. bytes, ".got");
    if (!got)
        return SPAWN_ERR_LOAD;

    // 6- Setup dispatcher
        // Fill ROOTCN_SLOT_TASKCN.TASKCN_SLOT_SELFEP
        // and ROOTCN_SLOT_TASKCN.TASKCN_SLOT_DISPATCHER
    struct capref dispatcher_ram;
    struct capref dispatcher_frame;
    struct capref dispatcher_endpoint;
    //TODO: Allocate dispatcher_frame and dispatcher_endpoint
    ERROR_RET1(ram_alloc(&dispatcher_ram, 1<<DISPATCHER_FRAME_BITS));
    ERROR_RET1(cap_retype(dispatcher_frame, dispatcher_ram, 0,
        ObjType_Frame, 1<<DISPATCHER_FRAME_BITS, 1));
	struct capref slot_dispatcher={
		.cnode=si->l2_cnodes[ROOTCN_SLOT_TASKCN],
		.slot=TASKCN_SLOT_DISPATCHER
	};
    struct capref slot_selfep={
        .cnode=si->l2_cnodes[ROOTCN_SLOT_TASKCN],
        .slot=TASKCN_SLOT_SELFEP
    };
    ERROR_RET1(cap_copy(slot_dispatcher, dispatcher_frame));
    ERROR_RET1(cap_retype(dispatcher_endpoint, dispatcher_frame, 0,
        ObjType_EndPoint, 1<<DISPATCHER_FRAME_BITS, 1));
    ERROR_RET1(cap_copy(slot_selfep, dispatcher_endpoint));

    struct dispatcher_shared_generic *disp =
        get_dispatcher_shared_generic(dispatcher_frame);
    struct dispatcher_generic *disp_gen = get_dispatcher_generic(dispatcher_frame);
    struct dispatcher_shared_arm *disp_arm =
        get_dispatcher_shared_arm(dispatcher_frame);
    arch_registers_state_t *enabled_area =
        dispatcher_get_enabled_save_area(dispatcher_frame);
    arch_registers_state_t *disabled_area =
        dispatcher_get_disabled_save_area(dispatcher_frame);

    disp_gen->core_id = my_core_id; // core id of the process
    disp->udisp = 0; // TODO: Virtual address of the dispatcher frame in childs VSpace
    disp->disabled = 1; // Start in disabled mode
    disp->fpu_trap = 1; // Trap on fpu instructions
    strncpy(disp->name, binary_name, DISP_NAME_LEN); // A name (for debugging)
    // TODO: Map, and give address in child's space?
    disabled_area->named.pc = child_entry_point; // Set program counter (where it should start to execute)
    // Initialize offset registers
    disp_arm->got_base = got; // Address of .got in childs VSpace.
    enabled_area->regs[REG_OFFSET(PIC_REGISTER)] = got; // same as above
    disabled_area->regs[REG_OFFSET(PIC_REGISTER)] = got; // same as above
    6enabled_area->named.cpsr = CPSR_F_MASK | ARM_MODE_USR;
    disabled_area->named.cpsr = CPSR_F_MASK | ARM_MODE_USR;
    disp_gen->eh_frame = 0;
    disp_gen->eh_frame_size = 0;
    disp_gen->eh_frame_hdr = 0;
    disp_gen->eh_frame_hdr_size = 0;

    // 7- Setup environment
    // 8- Make dispatcher runnable

    return SYS_ERR_OK;
}

errval_t elf_allocator(void *state, genvaddr_t base, size_t size, uint32_t flags, void **ret)
{
    debug_printf("elf_allocator");
    struct capref cap;
    ERROR_RET1(ram_alloc(&cap, size));
    // TODO
    return SYS_ERR_OK;
}
