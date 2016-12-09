#include <aos/aos.h>
#include <spawn/spawn.h>

#include <elf/elf.h>
#include <aos/dispatcher_arch.h>
#include <barrelfish_kpi/paging_arm_v7.h>
#include <barrelfish_kpi/domain_params.h>
#include <spawn/multiboot.h>


extern struct bootinfo *bi;
errval_t elf_allocator(void *state, genvaddr_t base, size_t size, uint32_t flags, void **ret);

errval_t spawn_map_multiboot(struct spawninfo* si, void** address);
errval_t spawn_setup_cspace(struct spawninfo* si);
errval_t spawn_setup_vspace(struct spawninfo* si);

// Child paging
errval_t spawn_setup_minimal_child_paging(struct spawninfo* si);
struct capref spawn_paging_alloc_child_slot(struct spawninfo* si, int count);

// Child slot alloc
errval_t spawn_child_slot_alloc(struct slot_allocator *ca, struct capref *cap);
errval_t spawn_child_slot_free(struct slot_allocator *ca, struct capref cap);


errval_t spawn_setup_dispatcher(struct spawninfo* si, struct lmp_chan* lc   );
errval_t spawn_setup_arguments(struct spawninfo* si, char* const argv[], int argc);
errval_t spawn_parse_elf(struct spawninfo* si, lvaddr_t address);

//Util functions
errval_t map_argument_to_child_vspace(char* const argv[], int argc, struct spawn_domain_params* child_args, lvaddr_t child_base_address);

errval_t spawn_load_with_args(char* const argv[], int argc, struct spawninfo * si,
        struct lmp_chan* lc)
{
    debug_printf("spawn_load_with_args with %d arguments\n", argc);
    for (int i = 0; i < argc; ++i)
        debug_printf("argv[%d] = '%s'\n", i, argv[i]);

    const char* binary_name = argv[0];
    debug_printf("spawn start_child: starting: %s, trying to load module\n", binary_name);
    if (si->core_id >= 2) // We only have 2 cores
        return SPAWN_ERR_WRONG_CORE_ID;

    // 1- Get the binary from multiboot image
    struct mem_region* process_mem_reg;
    debug_printf("calling spawn_load_module with si [0x%08x], name %s, mem reg [0x%08x] \n", si, binary_name, &process_mem_reg);
    ERROR_RET1(spawn_load_module(si, (const char*)binary_name, &process_mem_reg));
    debug_printf("----- spawn: mem region: [0x%08x] size: [0x%08x]\n", process_mem_reg, si->module_bytes);

    // 2- Map multiboot module in your address space
    void* address = NULL;
    ERROR_RET1(spawn_map_multiboot(si, &address));

    // 3- Setup childs cspace
    ERROR_RET1(spawn_setup_cspace(si));

    // 4- Setup childs vspace
    ERROR_RET1(spawn_setup_vspace(si));
    ERROR_RET1(spawn_setup_minimal_child_paging(si));

    // 5- Load the ELF binary
    debug_printf("Load ELF...\n");
    ERROR_RET1(spawn_parse_elf(si, (lvaddr_t)address));

    // 6- Setup dispatcher
    debug_printf("Setup dispatcher...\n");
    ERROR_RET1(spawn_setup_dispatcher(si, lc));

    // 7- Setup arguments
    debug_printf("Setup arguments...\n");
    ERROR_RET1(spawn_setup_arguments(si, argv, argc));

    // 8- Make dispatcher runnable0
    debug_printf("And finally invoke dispatcher :)\n");
	struct capref slot_dispatcher={
		.cnode=si->l2_cnodes[ROOTCN_SLOT_TASKCN],
		.slot=TASKCN_SLOT_DISPFRAME
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
    si->l1_pagetable_child_cap.slot = PAGECN_SLOT_VROOT;

    ERROR_RET1(slot_alloc(&si->l1_pagetable_own_cap));
    // Allocate L1 arm vnode
    ERROR_RET2(vnode_create(si->l1_pagetable_own_cap, ObjType_VNode_ARM_l1),
        SPAWN_ERR_L1_VNODE_CREATE);
    ERROR_RET1(cap_copy(si->l1_pagetable_child_cap, si->l1_pagetable_own_cap));
    debug_printf("Created child L1 pagetable\n");
    return SYS_ERR_OK;
}

/**
 * This function creates the first L2 pagetable.
 * We assume that we will not need more one L1 pagetable entry
 * during the spawn process.
 */
errval_t spawn_setup_minimal_child_paging(struct spawninfo* si)
{
    // Index 0 is reserved for L1 PT, so we start adding l2 pt from slot 1
    si->slot_alloc.pagecn_next_slot = PAGECN_SLOT_VROOT + 1;
    si->slot_alloc.a.alloc = spawn_child_slot_alloc;
    si->slot_alloc.a.free = spawn_child_slot_free;
    si->slot_alloc.pagecn = si->l2_cnodes[ROOTCN_SLOT_PAGECN];

    ERROR_RET1(paging_init_state(&si->child_paging_state,
        BASE_PAGE_SIZE,
        si->l1_pagetable_own_cap,
        (struct slot_allocator*)&si->slot_alloc));
    return SYS_ERR_OK;
}

errval_t spawn_child_slot_alloc(struct slot_allocator *ca, struct capref *cap)
{
    struct child_slot_allocator *sla = (struct child_slot_allocator *)ca;
    cap->cnode = sla->pagecn;
    cap->slot = sla->pagecn_next_slot;
    sla->pagecn_next_slot++;
    assert(sla->pagecn_next_slot <= L2_CNODE_SLOTS);
    return SYS_ERR_OK;
}

errval_t spawn_child_slot_free(struct slot_allocator *ca, struct capref cap)
{
    return SYS_ERR_OK;
}

errval_t spawn_setup_cspace(struct spawninfo* si)
{
	debug_printf("Setting up cspace for %s\n", si->binary_name);
    struct cnoderef cnoderef;
    ERROR_RET2(cnode_create_l1(&si->l1_cnode_cap, &cnoderef), SPAWN_ERR_SETUP_CSPACE);

    // FIXME we have a problem here
    debug_printf("L1 cnode: 0x%x, croot: 0x%x, slot: %d\n", si->l1_cnode_cap.cnode.cnode, &si->l1_cnode_cap.cnode.croot, &si->l1_cnode_cap.slot);
    for (int i = 0; i < ROOTCN_SLOTS_USER; ++i)
    {
        ERROR_RET2(cnode_create_foreign_l2(si->l1_cnode_cap,
                i, &si->l2_cnodes[i]), SPAWN_ERR_SETUP_CSPACE);
        debug_printf("L2 %d cnode: 0x%x, croot: 0x%x\n", i, si->l2_cnodes[i].cnode, si->l2_cnodes[i].croot);
    }

    // Fill capabilities
    // ROOTCN_SLOT_TASKCN.TASKCN_SLOT_ROOTCN
    struct capref child_rootcn;
    child_rootcn.cnode = si->l2_cnodes[ROOTCN_SLOT_TASKCN];
    child_rootcn.slot = TASKCN_SLOT_ROOTCN;
    ERROR_RET2(cap_copy(child_rootcn, si->l1_cnode_cap),
        SPAWN_ERR_MINT_ROOTCN);

    struct capref child_frame_ref;
    child_frame_ref.cnode = si->l2_cnodes[ROOTCN_SLOT_BASE_PAGE_CN];
    child_frame_ref.slot = 0;
    struct capref page_ref;
    ERROR_RET2(ram_alloc(&page_ref, BASE_PAGE_SIZE * L2_CNODE_SLOTS), SPAWN_ERR_CREATE_SMALLCN);
    ERROR_RET2(cap_retype(child_frame_ref,
        page_ref,
        0,
        ObjType_RAM,
        BASE_PAGE_SIZE,
        L2_CNODE_SLOTS), SPAWN_ERR_CREATE_SMALLCN);

    return SYS_ERR_OK;
}

errval_t spawn_setup_dispatcher(struct spawninfo* si, struct lmp_chan* lc)
{
    // I. Create dispatcher and endpoint
    struct capref dispatcher_endpoint;
    ERROR_RET1(slot_alloc(&si->child_dispatcher_own_cap));
    ERROR_RET1(slot_alloc(&dispatcher_endpoint));
    ERROR_RET1(dispatcher_create(si->child_dispatcher_own_cap));
    ERROR_RET1(cap_retype(dispatcher_endpoint, si->child_dispatcher_own_cap, 0,
        ObjType_EndPoint, 0, 1));

    // II. Create dispatcher frame cap
    struct capref ram_for_dispatcher;
    ERROR_RET1(slot_alloc(&si->child_dispatcher_frame_own_cap));
    ERROR_RET1(ram_alloc(&ram_for_dispatcher, DISPATCHER_SIZE));
    ERROR_RET1(cap_retype(si->child_dispatcher_frame_own_cap,
        ram_for_dispatcher, 0,
        ObjType_Frame, DISPATCHER_SIZE, 1));

    // III. Copy these caps to child CSpace
	struct capref slot_dispatcher={
		.cnode=si->l2_cnodes[ROOTCN_SLOT_TASKCN],
		.slot=TASKCN_SLOT_DISPATCHER
	};

    struct capref slot_selfep={
        .cnode=si->l2_cnodes[ROOTCN_SLOT_TASKCN],
        .slot=TASKCN_SLOT_SELFEP
    };

	struct capref slot_dispatcher_frame={
		.cnode=si->l2_cnodes[ROOTCN_SLOT_TASKCN],
		.slot=TASKCN_SLOT_DISPFRAME
	};

	// new in milestone 3 - give the child an endpoint back to init so it can finish the handshake
    struct capref slot_parent_endpoint={
        .cnode=si->l2_cnodes[ROOTCN_SLOT_TASKCN],
        .slot=TASKCN_SLOT_INITEP
    };

    ERROR_RET1(cap_copy(slot_dispatcher, si->child_dispatcher_own_cap));
    ERROR_RET1(cap_copy(slot_selfep, dispatcher_endpoint));
    ERROR_RET1(cap_copy(slot_dispatcher_frame, si->child_dispatcher_frame_own_cap));
    ERROR_RET1(cap_copy(slot_parent_endpoint, lc->local_cap));

    // IV. Map in child process
    // Map dispatcher frame for child
    ERROR_RET1(paging_map_frame(&si->child_paging_state,
        (void**)&si->dispatcher_frame_mapped_child, DISPATCHER_SIZE,
        si->child_dispatcher_frame_own_cap,
        NULL, NULL));
    // Map dispatcher frame for me
    void* disp_mapped_me;
	ERROR_RET1(paging_map_frame(get_current_paging_state(),
        &disp_mapped_me, DISPATCHER_SIZE,
		si->child_dispatcher_frame_own_cap,
        NULL, NULL));
    si->dispatcher_handle = (dispatcher_handle_t)disp_mapped_me;

    // V. Setup dispatcher values
    struct dispatcher_shared_generic *disp =
        get_dispatcher_shared_generic(si->dispatcher_handle);
    struct dispatcher_generic *disp_gen = get_dispatcher_generic(si->dispatcher_handle);
    struct dispatcher_shared_arm *disp_arm =
        get_dispatcher_shared_arm(si->dispatcher_handle);
    si->enabled_area = dispatcher_get_enabled_save_area(si->dispatcher_handle);
    arch_registers_state_t *disabled_area =
        dispatcher_get_disabled_save_area(si->dispatcher_handle);

        // my_core_id only usable from kernel!!
    disp_gen->core_id = si->core_id;
    disp->udisp = si->dispatcher_frame_mapped_child; // Virtual address of the dispatcher frame in childs VSpace
    disp->disabled = 1; // Start in disabled mode
    disp->fpu_trap = 1; // Trap on fpu instructions
    strncpy(disp->name, si->binary_name, DISP_NAME_LEN); // A name (for debugging)
    disabled_area->named.pc = si->child_entry_point; // Set program counter (where it should start to execute)
    // Initialize offset registers
    disp_arm->got_base = si->got; // Address of .got in childs VSpace.
    si->enabled_area->regs[REG_OFFSET(PIC_REGISTER)] = si->got; // same as above
    disabled_area->regs[REG_OFFSET(PIC_REGISTER)] = si->got; // same as above
    si->enabled_area->named.cpsr = CPSR_F_MASK | ARM_MODE_USR;
    disabled_area->named.cpsr = CPSR_F_MASK | ARM_MODE_USR;
    disp_gen->eh_frame = 0;
    disp_gen->eh_frame_size = 0;
    disp_gen->eh_frame_hdr = 0;
    disp_gen->eh_frame_hdr_size = 0;
    return SYS_ERR_OK;
}

errval_t map_argument_to_child_vspace(char* const argv[], int argc,
        struct spawn_domain_params* child_args, lvaddr_t child_base_address)
{
    assert(argc > 0 && "Argument list must contain at least one argument, program name");

    // Mapping:
    //  parent: child_args
    //  child:  child_base_address

    // Current position relative to structure begin
    size_t cur_position = sizeof(struct spawn_domain_params);
    void* my_buf = child_args;
    child_args->argc = argc;
    for (int i = 0; i < argc; ++i)
    {
        child_args->argv[i] = (char*)(child_base_address + cur_position);
        size_t this_size = strlen(argv[i]) + 1;
        memcpy(my_buf + cur_position, argv[i], this_size);
        cur_position += this_size;
    }
    return SYS_ERR_OK;
}

errval_t spawn_setup_arguments(struct spawninfo* si, char* const argv[], int argc)
{
    size_t args_length = 0;
    for (int i = 0; i < argc; ++i)
        args_length += strlen(argv[i]) + 1;

    size_t domain_params_frame_size = sizeof(struct spawn_domain_params);
    domain_params_frame_size += args_length;
    domain_params_frame_size = ROUND_UP(domain_params_frame_size, BASE_PAGE_SIZE);

    // Get a frame with enough space to store that.
    void* args_page;
    struct capref ram_cap;
    struct capref frame_cap;
    ERROR_RET1(ram_alloc(&ram_cap, domain_params_frame_size));
    ERROR_RET1(slot_alloc(&frame_cap));
    ERROR_RET1(cap_retype(frame_cap, ram_cap, 0,
        ObjType_Frame, domain_params_frame_size, 1));
    ERROR_RET1(paging_map_frame(get_current_paging_state(), &args_page,
        domain_params_frame_size, frame_cap, NULL, NULL));

    struct capref slot_arguments_page={
        .cnode=si->l2_cnodes[ROOTCN_SLOT_TASKCN],
        .slot=TASKCN_SLOT_ARGSPAGE
    };

    void* foreign_mapped_args;
    ERROR_RET1(slot_alloc(&si->child_arguments_frame_own_cap));
    ERROR_RET1(cap_copy(si->child_arguments_frame_own_cap, frame_cap));
    ERROR_RET1(paging_map_frame(&si->child_paging_state, &foreign_mapped_args,
            domain_params_frame_size, si->child_arguments_frame_own_cap, NULL, NULL));

    ERROR_RET1(cap_copy(slot_arguments_page, si->child_arguments_frame_own_cap));

    struct spawn_domain_params* child_args=(struct spawn_domain_params*)args_page;
    // Fill initial values
    memset(&child_args->argv[0], 0, sizeof(child_args->argv));
    memset(&child_args->envp[0], 0, sizeof(child_args->envp));
    //Copy first argument, name of the program

    map_argument_to_child_vspace(argv, argc, child_args, (lvaddr_t)foreign_mapped_args);

    // Don't care about this for now.
    child_args->vspace_buf = NULL;   //Not sure if this should be used in this way
    child_args->vspace_buf_len = 0;  //or this
    child_args->tls_init_base = NULL;
    child_args->tls_init_len = 0;
    child_args->tls_total_len = 0;
    child_args->pagesize = 0;

    si->enabled_area->named.r0 = (uint32_t)foreign_mapped_args;
    return SYS_ERR_OK;
}

errval_t spawn_parse_elf(struct spawninfo* si, lvaddr_t address)
{
    debug_printf("Loading ELF binary...\n");
    ERROR_RET2(elf_load(EM_ARM, elf_allocator,
        (void*)si, address,
        si->module_bytes, &si->child_entry_point),
        SPAWN_ERR_LOAD);
    debug_printf("elf32_find_section_header_name...\n");
    struct Elf64_Ehdr *head = (struct Elf64_Ehdr *)address;
    debug_printf("Ident: %u\n", head->e_ident[EI_CLASS]);
    struct Elf32_Shdr *got = elf32_find_section_header_name((lvaddr_t)address,
        si->module_bytes, ".got");
    if (!got)
        return SPAWN_ERR_LOAD;
    si->got = got->sh_addr;
    return SYS_ERR_OK;
}

errval_t elf_allocator(void *state, genvaddr_t base, size_t size, uint32_t flags, void **ret)
{
    debug_printf("elf_allocator: invoked, request for : %lu bytes at 0x%08x\n", size, (int) base);

    struct spawninfo* si = (struct spawninfo*)state;

    // 1. Fix sizes / alignments etc...
    size_t base_offset = BASE_PAGE_OFFSET(base);
    size += base_offset;
    base -= base_offset;
    size = ROUND_UP(size, BASE_PAGE_SIZE);

    // 2. Allocate frame
    struct capref ram_ref;
	ERROR_RET1(ram_alloc(&ram_ref, size));
	struct capref frame_cap;
	slot_alloc(&frame_cap);
	ERROR_RET1(cap_retype(frame_cap, ram_ref, 0,
				ObjType_Frame, size, 1));

    // 3. Map in my own space and fill return buffer
	struct paging_state* ps= get_current_paging_state();


	ERROR_RET1(paging_map_frame(ps, ret, size, frame_cap, NULL, NULL));

    *ret = (((void*)*ret) + (size_t)base_offset);
    debug_printf("Allocated at 0x%08x\n", (int)*ret);

    // 4. Map in child VSpace at given 'base' address
    ERROR_RET1(paging_alloc_fixed_address(&si->child_paging_state,
            base, size));
    ERROR_RET1(paging_map_fixed(&si->child_paging_state,
        base, frame_cap, size));

    return SYS_ERR_OK;
}

errval_t spawn_map_multiboot(struct spawninfo* si, void** address)
{
    struct paging_state* page_state=get_current_paging_state();
	ERROR_RET2(paging_map_frame_attr(page_state, address,
            si->module_bytes,
			si->module_frame, VREGION_FLAGS_READ_WRITE,
            NULL, NULL),
            SPAWN_ERR_MAP_MODULE);

    char* elf = (char*)address;
    debug_printf("Beginning at 0x%x: 0x%x 0x%x 0x%x 0x%x. Size=%u\n",
        (int)address, elf[0], elf[1], elf[2], elf[3],
        si->module_bytes);
    return SYS_ERR_OK;
}
