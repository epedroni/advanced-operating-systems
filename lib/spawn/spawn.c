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

// Child paging
errval_t spawn_setup_minimal_child_paging(struct spawninfo* si);
errval_t spawn_paging_add_l2_pt(struct spawninfo* si);
struct capref spawn_paging_alloc_child_slot(struct spawninfo* si, int count);
errval_t spawn_paging_map_child_process(struct spawninfo* si,
            struct capref frame, lvaddr_t* address, size_t bytes);
errval_t spawn_paging_map_child_process_with_offset(struct spawninfo* si,
            struct capref frame, lvaddr_t* address, size_t offset, size_t bytes);

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
    ERROR_RET1(spawn_setup_minimal_child_paging(si));

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

    debug_printf("Received address of mem_region: 0x%X\n", *process_mem_reg);

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
    ERROR_RET1(paging_init_state(&si->child_paging_state,
        VADDR_OFFSET,
        si->l1_pagetable_own_cap,
        get_default_slot_allocator()));
    si->next_virtual_address = VADDR_OFFSET;
    si->pagecn_next_slot = 1; // Index 0 is reserved for L1 PT, so we start adding l2 pt from slot 1
    si->current_l1_slot = ARM_L1_OFFSET(si->next_virtual_address) - 1;
    return spawn_paging_add_l2_pt(si);
}

errval_t spawn_paging_add_l2_pt(struct spawninfo* si)
{
    ++si->current_l1_slot;
    // I. Create L2 Pagetable
    ERROR_RET1(slot_alloc(&si->child_l2_pt_own_cap));
    ERROR_RET1(vnode_create(si->child_l2_pt_own_cap, ObjType_VNode_ARM_l2));

    // II. Map child L2 -> child L1 for base virtual address
    struct capref l2_to_l1_mapping_own_cap;
    ERROR_RET1(slot_alloc(&l2_to_l1_mapping_own_cap));
    ERROR_RET1(vnode_map(si->l1_pagetable_own_cap, si->child_l2_pt_own_cap,
    		ARM_L1_OFFSET(si->next_virtual_address), VREGION_FLAGS_READ_WRITE,
                    0, 1, l2_to_l1_mapping_own_cap));

    // III. Copy all these caps to child process
    struct capref l2_arm_vtable = spawn_paging_alloc_child_slot(si, 1);
    struct capref l2_to_l1_mapping = spawn_paging_alloc_child_slot(si, 1);
    ERROR_RET1(cap_copy(l2_arm_vtable, si->child_l2_pt_own_cap));
    ERROR_RET1(cap_copy(l2_to_l1_mapping, l2_to_l1_mapping_own_cap));

    return SYS_ERR_OK;
}

struct capref spawn_paging_alloc_child_slot(struct spawninfo* si, int count)
{
    struct capref cap;
    cap.cnode = si->l2_cnodes[ROOTCN_SLOT_PAGECN];
    cap.slot = si->pagecn_next_slot;
    si->pagecn_next_slot += count;
    assert(si->pagecn_next_slot <= L2_CNODE_SLOTS);
    return cap;
}

errval_t spawn_paging_map_child_process(struct spawninfo* si,
            struct capref frame, lvaddr_t* address, size_t bytes)
{
    return spawn_paging_map_child_process_with_offset(si, frame, address, 0, bytes);
}

errval_t spawn_paging_map_child_process_with_offset(struct spawninfo* si,
            struct capref frame, lvaddr_t* address, size_t offset, size_t bytes)
{
    bytes = ROUND_UP(bytes, BASE_PAGE_SIZE);
    size_t num_pages = bytes >> BASE_PAGE_BITS;
    *address = si->next_virtual_address;

    if (si->current_l1_slot != ARM_L1_OFFSET(si->next_virtual_address))
    {
        ERROR_RET1(spawn_paging_add_l2_pt(si));
        assert(si->current_l1_slot == ARM_L1_OFFSET(si->next_virtual_address));
    }

    capaddr_t l1_slot = ARM_L1_OFFSET(si->next_virtual_address);
    capaddr_t l1_slot_end = ARM_L1_OFFSET(si->next_virtual_address + bytes - 1);
    if (l1_slot != l1_slot_end)
    {
        lvaddr_t unused_addr;
        for (; l1_slot < l1_slot_end; ++l1_slot)
        {
            size_t bytes_this_l1 = LARGE_PAGE_SIZE -
                (ARM_L2_OFFSET(si->next_virtual_address) << BASE_PAGE_BITS);
            ERROR_RET1(spawn_paging_map_child_process_with_offset(si, frame, &unused_addr, offset, bytes_this_l1));
            offset += bytes_this_l1;
            bytes -= bytes_this_l1;
        }
        return spawn_paging_map_child_process_with_offset(si, frame, &unused_addr, offset, bytes);
    }

    assert(ARM_L1_OFFSET(si->next_virtual_address) ==
        ARM_L1_OFFSET(si->next_virtual_address + bytes - 1) &&
        "At this point, we should only map into a single L2!");

    // Map the frame
    struct capref mapping;
    ERROR_RET1(slot_alloc(&mapping));
    ERROR_RET1(vnode_map(si->child_l2_pt_own_cap, frame,
            ARM_L2_OFFSET(si->next_virtual_address), VREGION_FLAGS_READ_WRITE,
                offset, num_pages, mapping));

    // And copy the frame mapping to child CSpace
    ERROR_RET1(cap_copy(spawn_paging_alloc_child_slot(si, 1), mapping));
    si->next_virtual_address += bytes;
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
        ERROR_RET2(cap_copy(child_frame_ref, page_ref), SPAWN_ERR_CREATE_SMALLCN);
    }
    return SYS_ERR_OK;
}

errval_t spawn_setup_dispatcher(struct spawninfo* si)
{
    // I. Create dispatcher and endpoint
    struct capref dispatcher_endpoint;
    ERROR_RET1(slot_alloc(&si->child_dispatcher_own_cap));
    ERROR_RET1(slot_alloc(&dispatcher_endpoint));
    ERROR_RET1(dispatcher_create(si->child_dispatcher_own_cap));
    ERROR_RET1(cap_retype(dispatcher_endpoint, si->child_dispatcher_own_cap, 0,
        ObjType_EndPoint, 1<<DISPATCHER_FRAME_BITS, 1));

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
    ERROR_RET1(cap_copy(slot_dispatcher, si->child_dispatcher_own_cap));
    ERROR_RET1(cap_copy(slot_selfep, dispatcher_endpoint));
    ERROR_RET1(cap_copy(slot_dispatcher_frame, si->child_dispatcher_frame_own_cap));

    // IV. Map in child process
    // Map dispatcher frame for child
    ERROR_RET1(spawn_paging_map_child_process(si,
        si->child_dispatcher_frame_own_cap,
        &si->dispatcher_frame_mapped_child,
        DISPATCHER_SIZE));
    // Map dispatcher frame for me
    void* disp_mapped_me;
	ERROR_RET1(paging_map_frame_attr(get_current_paging_state(),
            &disp_mapped_me, DISPATCHER_SIZE,
			si->child_dispatcher_frame_own_cap, VREGION_FLAGS_READ_WRITE,
            NULL, NULL));
    si->dispatcher_handle = (dispatcher_handle_t)disp_mapped_me;

    // V. Setup dispatcher values
    struct dispatcher_shared_generic *disp =
        get_dispatcher_shared_generic(si->dispatcher_handle);
    struct dispatcher_generic *disp_gen = get_dispatcher_generic(si->dispatcher_handle);
    struct dispatcher_shared_arm *disp_arm =
        get_dispatcher_shared_arm(si->dispatcher_handle);
    arch_registers_state_t *enabled_area =
        dispatcher_get_enabled_save_area(si->dispatcher_handle);
    arch_registers_state_t *disabled_area =
        dispatcher_get_disabled_save_area(si->dispatcher_handle);

        // my_core_id only usable from kernel!!
    disp_gen->core_id = 0; // TODO: core id of the process
    disp->udisp = si->dispatcher_frame_mapped_child; // Virtual address of the dispatcher frame in childs VSpace
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
    domain_params_frame_size = ROUND_UP(domain_params_frame_size, BASE_PAGE_SIZE);

    // Get a frame with enough space to store that.
    struct spawn_domain_params* child_args;
    struct capref frame;
    struct capref ram_cap;
    ERROR_RET1(ram_alloc(&ram_cap, domain_params_frame_size));
    ERROR_RET1(slot_alloc(&frame));
    ERROR_RET1(cap_retype(frame, ram_cap, 0, ObjType_Frame, domain_params_frame_size, 1));
    ERROR_RET1(paging_map_frame(get_current_paging_state(), (void*)&child_args,
        domain_params_frame_size, frame, NULL, NULL));

    // Fill initial values
    child_args->argc = 0;
    memset(&child_args->argv[0], 0, sizeof(child_args->argv));
    memset(&child_args->envp[0], 0, sizeof(child_args->envp));
    // Don't care about this for now.
    child_args->vspace_buf = NULL;
    child_args->vspace_buf_len = 0;
    child_args->tls_init_base = NULL;
    child_args->tls_init_len = 0;
    child_args->tls_total_len = 0;
    child_args->pagesize = 0;

    // TODO: Setup arguments
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
    si->got = (lvaddr_t)got;
debug_printf("Got it...\n");
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
	lvaddr_t virtual_address;
	ERROR_RET1(spawn_paging_map_child_process(si, frame_cap, &virtual_address, size));
	struct paging_state* ps= get_current_paging_state();
	ERROR_RET1(paging_map_frame(ps, ret, size, frame_cap, NULL, NULL));

    *ret = (((void*)*ret) + (size_t)base_offset);
    debug_printf("Allocated at 0x%08x\n", (int)*ret);

    // 4. Map in child VSpace at given 'base' address
    // TODO: ???

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
