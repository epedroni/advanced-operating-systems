#include <aos/aos.h>
#include <spawn/spawn.h>

#include <elf/elf.h>
#include <aos/dispatcher_arch.h>
#include <barrelfish_kpi/paging_arm_v7.h>
#include <barrelfish_kpi/domain_params.h>
#include <spawn/multiboot.h>


extern struct bootinfo *bi;
errval_t elf_allocator(void *state, genvaddr_t base, size_t size, uint32_t flags, void **ret);

errval_t spawn_load_module(struct spawninfo* si, const char* binary_name, struct mem_region** process_mem_reg);
errval_t spawn_map_multiboot(struct spawninfo* si, void** address);
errval_t spawn_setup_cspace(struct spawninfo* si);
errval_t spawn_setup_vspace(struct spawninfo* si);
errval_t spawn_setup_dispatcher(struct spawninfo* si);
errval_t spawn_setup_arguments(struct spawninfo* si, struct mem_region* process_mem_reg);
errval_t spawn_parse_elf(struct spawninfo* si, lvaddr_t address);

// TODO(M2): Implement this function such that it starts a new process
// TODO(M4): Build and pass a messaging channel to your child process
errval_t spawn_load_by_name(void * binary_name, struct spawninfo * si) {
    printf("spawn start_child: starting: %s\n", binary_name);

    // 1- Get the binary from multiboot image
    struct mem_region* process_mem_reg;
    ERROR_RET1(spawn_load_module(si, (const char*)binary_name, &process_mem_reg));

    // 2- Map multiboot module in your address space
    void* address = NULL;
    ERROR_RET1(spawn_map_multiboot(si, &address));

    // 3- Setup childs cspace
    ERROR_RET1(spawn_setup_cspace(si));

    // 4- Setup childs vspace
    ERROR_RET1(spawn_setup_vspace(si));

    // 5- Load the ELF binary
    ERROR_RET1(spawn_parse_elf(si, (lvaddr_t)address));

    // 6- Setup dispatcher
    ERROR_RET1(spawn_setup_dispatcher(si));

    // 7- Setup arguments
    ERROR_RET1(spawn_setup_arguments(si, process_mem_reg));

    // 8- Make dispatcher runnable
	struct capref slot_dispatcher={
		.cnode=si->l2_cnodes[ROOTCN_SLOT_TASKCN],
		.slot=TASKCN_SLOT_DISPATCHER
	};
    return invoke_dispatcher(si->child_dispatcher_own_cap, cap_dispatcher,
                  si->l1_cnode_cap, si->l1_pagetable_child_cap,
                  slot_dispatcher, true);
}

errval_t spawn_load_module(struct spawninfo* si, const char* binary_name, struct mem_region** process_mem_reg)
{
    *process_mem_reg=multiboot_find_module(bi, binary_name);

    if (!*process_mem_reg)
        return SPAWN_ERR_FIND_MODULE;

    memset(si, 0, sizeof(*si));
    si->binary_name = malloc(strlen(binary_name) + 1);
    strcpy(si->binary_name, binary_name);
    si->base_virtual_address=VADDR_OFFSET;

    debug_printf("Received address of mem_region: 0x%X\n",*process_mem_reg);

    // Binary frame
    si->module_frame.cnode = cnode_module;
    si->module_frame.slot = (*process_mem_reg)->mrmod_slot;
    struct frame_identity spawned_process_frame_id;
    ERROR_RET2(frame_identify(si->module_frame, &spawned_process_frame_id),
            SPAWN_ERR_MAP_MODULE);
    si->module_bytes = spawned_process_frame_id.bytes;
    return SYS_ERR_OK;
}

errval_t spawn_setup_vspace(struct spawninfo* si)
{
    // Slot 0 contains L1 PageTable
    si->l1_pagetable_child_cap.cnode = si->l2_cnodes[ROOTCN_SLOT_PAGECN];
    si->l1_pagetable_child_cap.slot = 0;

    // Allocate L1 arm vnode
    ERROR_RET2(vnode_create(si->l1_pagetable_child_cap, ObjType_VNode_ARM_l1),
        SPAWN_ERR_L1_VNODE_CREATE);
    debug_printf("Created child L1 pagetable\n");
    return SYS_ERR_OK;
}

errval_t spawn_setup_cspace(struct spawninfo* si)
{
    struct cnoderef cnoderef;
    ERROR_RET2(cnode_create_l1(&si->l1_cnode_cap, &cnoderef), SPAWN_ERR_SETUP_CSPACE);

    for (int i = 0; i < ROOTCN_SLOTS_USER; ++i)
    {
        ERROR_RET2(cnode_create_foreign_l2(si->l1_cnode_cap,
                i, &si->l2_cnodes[i]), SPAWN_ERR_SETUP_CSPACE);
    }

    // Fill capabilities
    // ROOTCN_SLOT_TASKCN.TASKCN_SLOT_ROOTCN
    struct capref child_rootcn;
    child_rootcn.cnode = si->l2_cnodes[ROOTCN_SLOT_TASKCN];
    child_rootcn.slot = TASKCN_SLOT_ROOTCN;
    ERROR_RET2(cap_copy(child_rootcn, si->l1_cnode_cap),
        SPAWN_ERR_MINT_ROOTCN);
        // TASKCN_SLOT_DISPFRAME ??
        // TASKCN_SLOT_ARGSPAGE ??

    struct capref child_frame_ref;
    child_frame_ref.cnode = si->l2_cnodes[ROOTCN_SLOT_BASE_PAGE_CN];
    for (child_frame_ref.slot = 0; child_frame_ref.slot < L2_CNODE_SLOTS; ++child_frame_ref.slot)
    {
        struct capref page_ref;
        ERROR_RET2(ram_alloc(&page_ref, BASE_PAGE_SIZE), SPAWN_ERR_CREATE_SMALLCN);
        //TODO: RAM or Frame?
        ERROR_RET2(cap_copy(child_frame_ref, page_ref), SPAWN_ERR_CREATE_SMALLCN);
    }
    return SYS_ERR_OK;
}

errval_t spawn_setup_dispatcher(struct spawninfo* si)
{
    struct capref dispatcher_endpoint;
    //TODO: Allocate slots for child_dispatcher_own_cap, dispatcher_endpoint
    ERROR_RET1(dispatcher_create(si->child_dispatcher_own_cap));
	struct capref slot_dispatcher={
		.cnode=si->l2_cnodes[ROOTCN_SLOT_TASKCN],
		.slot=TASKCN_SLOT_DISPATCHER
	};
    struct capref slot_selfep={
        .cnode=si->l2_cnodes[ROOTCN_SLOT_TASKCN],
        .slot=TASKCN_SLOT_SELFEP
    };
    ERROR_RET1(cap_copy(slot_dispatcher, si->child_dispatcher_own_cap));
    ERROR_RET1(cap_retype(dispatcher_endpoint, si->child_dispatcher_own_cap, 0,
        ObjType_EndPoint, 1<<DISPATCHER_FRAME_BITS, 1));
    ERROR_RET1(cap_copy(slot_selfep, dispatcher_endpoint));

    struct capref l2_arm_vtable={
        .cnode=si->l2_cnodes[ROOTCN_SLOT_PAGECN],
        .slot=1	//first slot for l2 pagetable
    };

    ERROR_RET1(vnode_create(l2_arm_vtable, ObjType_VNode_ARM_l2));

    struct capref l2_to_l1_mapping={
		.cnode=si->l2_cnodes[ROOTCN_SLOT_PAGECN],
		.slot=2	//second slot for l2 to l1 mapping
	};

    vnode_map(si->l1_pagetable_child_cap, l2_arm_vtable,
    		ARM_L1_OFFSET(si->base_virtual_address), VREGION_FLAGS_READ_WRITE,
                    0, 1, l2_to_l1_mapping);

    struct capref dispatcher_mapping={
		.cnode=si->l2_cnodes[ROOTCN_SLOT_PAGECN],
		.slot=3	//second slot for l2 to l1 mapping
	};

    // Map dispatcher
    vnode_map(l2_arm_vtable, dispatcher_cap,
    		ARM_L2_OFFSET(si->base_virtual_address), VREGION_FLAGS_READ_WRITE,
                0, 1, dispatcher_mapping);

    si->base_virtual_address+=BASE_PAGE_SIZE;


    struct dispatcher_shared_generic *disp =
        get_dispatcher_shared_generic(dispatcher_frame);
    struct dispatcher_generic *disp_gen = get_dispatcher_generic(dispatcher_frame);
    struct dispatcher_shared_arm *disp_arm =
        get_dispatcher_shared_arm(dispatcher_frame);
    arch_registers_state_t *enabled_area =
        dispatcher_get_enabled_save_area(dispatcher_frame);
    arch_registers_state_t *disabled_area =
        dispatcher_get_disabled_save_area(dispatcher_frame);

        // my_core_id only usable from kernel!!
    disp_gen->core_id = 0; // TODO: core id of the process
    disp->udisp = 0; // TODO: Virtual address of the dispatcher frame in childs VSpace
    disp->disabled = 1; // Start in disabled mode
    disp->fpu_trap = 1; // Trap on fpu instructions
    strncpy(disp->name, si->binary_name, DISP_NAME_LEN); // A name (for debugging)
    // TODO: Map, and give address in child's space?
    disabled_area->named.pc = si->child_entry_point; // Set program counter (where it should start to execute)
    // Initialize offset registers
    disp_arm->got_base = si->got; // Address of .got in childs VSpace.
    enabled_area->regs[REG_OFFSET(PIC_REGISTER)] = si->got; // same as above
    disabled_area->regs[REG_OFFSET(PIC_REGISTER)] = si->got; // same as above
    enabled_area->named.cpsr = CPSR_F_MASK | ARM_MODE_USR;
    disabled_area->named.cpsr = CPSR_F_MASK | ARM_MODE_USR;
    disp_gen->eh_frame = 0;
    disp_gen->eh_frame_size = 0;
    disp_gen->eh_frame_hdr = 0;
    disp_gen->eh_frame_hdr_size = 0;
    return SYS_ERR_OK;
}

errval_t spawn_setup_arguments(struct spawninfo* si, struct mem_region* process_mem_reg)
{
    const char* args = multiboot_module_opts(process_mem_reg);
    size_t args_length = strlen(args);
    size_t domain_params_frame_size = sizeof(struct spawn_domain_params);
    domain_params_frame_size += args_length + 1;
    struct spawn_domain_params child_args;
    child_args.argc = 0;
    memset(&child_args.argv[0], 0, sizeof(child_args.argv));
    memset(&child_args.envp[0], 0, sizeof(child_args.envp));
    child_args.vspace_buf = NULL;   // TODO: Serialised vspace data
    child_args.vspace_buf_len = 0;   // TODO: Length of serialised vspace data
    child_args.tls_init_base = NULL;     // TODO: Address of initialised TLS data block
    child_args.tls_init_len = 0;     // TODO: Length of initialised TLS data block
    child_args.tls_total_len = 0;   // TODO: Total (initialised + BSS) TLS data length
    child_args.pagesize = 0;        // TODO: the page size to be used (domain spanning)
    return SYS_ERR_OK;
}

errval_t spawn_parse_elf(struct spawninfo* si, lvaddr_t address)
{
    debug_printf("Loading ELF binary...\n");
    ERROR_RET2(elf_load(EM_ARM, elf_allocator,
        NULL, address,
        si->module_bytes, &si->child_entry_point),
        SPAWN_ERR_LOAD);
    debug_printf("elf32_find_section_header_name...\n");
    struct Elf32_Shdr* got = elf32_find_section_header_name((lvaddr_t)address,
        si->module_bytes, ".got");
    if (!got)
        return SPAWN_ERR_LOAD;
    si->got = (lvaddr_t)got;
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


errval_t spawn_map_multiboot(struct spawninfo* si, void** address)
{
    struct paging_state* page_state=get_current_paging_state();
	ERROR_RET2(paging_map_frame_attr(page_state, address, si->module_bytes,
			si->module_frame, VREGION_FLAGS_READ_WRITE, NULL, NULL),
            SPAWN_ERR_MAP_MODULE);

    char* elf = (char*)address;
    debug_printf("Beginning at 0x%x: 0x%x %c %c %c. Size=%u\n",
        (int)address, elf[0], elf[1], elf[2], elf[3],
        si->module_bytes);
    return SYS_ERR_OK;
}
