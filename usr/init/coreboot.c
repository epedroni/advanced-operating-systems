#include "coreboot.h"

errval_t write_to_urpc(void* urpc_buf, genpaddr_t base, gensize_t size,
        struct bootinfo* bi, coreid_t my_core_id)
{
    assert (my_core_id == 0);

    *((struct bootinfo*) urpc_buf) = *bi;

    urpc_buf = (void*) ROUND_UP((uintptr_t) urpc_buf + sizeof(struct bootinfo), 4);
    memcpy(urpc_buf, bi->regions, bi->regions_length * sizeof(struct mem_region));

    // mmstrings cap, for reading up modules.
    struct capref mmstrings_cap = {
        .cnode = cnode_module,
        .slot = 0
    };
    struct frame_identity mmstrings_id;
    ERROR_RET1(frame_identify(mmstrings_cap, &mmstrings_id));

    urpc_buf = (void*) ROUND_UP((uintptr_t) urpc_buf + bi->regions_length * sizeof(struct mem_region), 4);
    *((genpaddr_t*) urpc_buf) = mmstrings_id.base;
    urpc_buf += sizeof(genpaddr_t);
    *((gensize_t*) urpc_buf) = mmstrings_id.bytes;

    size_t non_zero_slot_count = 0;
    for (size_t i = 0; i < bi->regions_length; ++i) {
        if (bi->regions[i].mrmod_slot != 0) {
            ++non_zero_slot_count;
        }
    }
    urpc_buf += sizeof(gensize_t);
    *((size_t*) urpc_buf) = non_zero_slot_count;
    urpc_buf += sizeof(size_t);

    for (size_t i = 0; i < bi->regions_length; ++i) {
        if (bi->regions[i].mrmod_slot != 0) {
            struct capref module = {
                .cnode = cnode_module,
                .slot = bi->regions[i].mrmod_slot
            };
            struct frame_identity module_id;
            ERROR_RET1(frame_identify(module, &module_id));

            *((cslot_t*) urpc_buf) = module.slot;
            urpc_buf += sizeof(cslot_t);
            *((genpaddr_t*) urpc_buf) = module_id.base;
            urpc_buf += sizeof(genpaddr_t);
            *((gensize_t*) urpc_buf) = module_id.bytes;
            urpc_buf += sizeof(gensize_t);
        }
    }
    return SYS_ERR_OK;
}

errval_t read_from_urpc(void* urpc_buf, struct bootinfo** bi,
        coreid_t my_core_id)
{
    if (my_core_id == 0) {
        return SYS_ERR_OK;
    }

    debug_printf("Reading from urpc\n");
    *bi = (struct bootinfo*) urpc_buf;

    urpc_buf = (void*) ROUND_UP((uintptr_t) urpc_buf + sizeof(struct bootinfo), 4);
    memcpy((*bi)->regions, urpc_buf, (*bi)->regions_length * sizeof(struct mem_region));

    return SYS_ERR_OK;
}

errval_t read_modules(void* urpc_buf, struct bootinfo* bi,
        coreid_t my_core_id) {
    if (my_core_id == 0) {
        return SYS_ERR_OK;
    }
    debug_printf("read modules calculating address\n");
    // mmstrings cap, for reading up modules.
    urpc_buf = (void*) ROUND_UP((uintptr_t) urpc_buf + sizeof(struct bootinfo), 4);
    urpc_buf = (void*) ROUND_UP((uintptr_t) urpc_buf + bi->regions_length * sizeof(struct mem_region), 4);

    genpaddr_t* mmstrings_base = (genpaddr_t*) urpc_buf;
    urpc_buf += sizeof(genpaddr_t);
    gensize_t* mmstrings_size = (gensize_t*) urpc_buf;

    struct capref mmstrings_cap = {
        .cnode = cnode_module,
        .slot = 0
    };

    struct capref l1cnode = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_ROOTCN
    };

    debug_printf("create foriegn l2\n");
    ERROR_RET1(cnode_create_foreign_l2(l1cnode, ROOTCN_SLOT_MODULECN,&cnode_module));
    debug_printf("cnode_module: root: %u, cnode: %u, level: %u\n",
            cnode_module.croot, cnode_module.cnode >> 8, cnode_module.level);

    debug_printf("frame forge\n");
    ERROR_RET1(frame_forge(mmstrings_cap, *mmstrings_base, *mmstrings_size,
                my_core_id));

    urpc_buf += sizeof(gensize_t);
    size_t* non_zero_slot_count = (size_t*) urpc_buf;
    urpc_buf += sizeof(size_t);

    debug_printf("for loop\n");
    for (size_t i = 0; i < *non_zero_slot_count; ++i) {
        // Read slot, base and size & forge cap.
        cslot_t* module_slot = (cslot_t*) urpc_buf;
        urpc_buf += sizeof(cslot_t);
        genpaddr_t* module_base = (genpaddr_t*) urpc_buf;
        urpc_buf += sizeof(genpaddr_t);
        gensize_t* module_size = (gensize_t*) urpc_buf;
        urpc_buf += sizeof(gensize_t);

        struct capref module_cap = {
            .cnode = cnode_module,
            .slot = *module_slot
        };
        ERROR_RET1(frame_forge(module_cap, *module_base, *module_size,
                my_core_id));
    }

    return SYS_ERR_OK;
}

errval_t coreboot_init(struct bootinfo *bi){
    debug_printf("---- starting coreboot init ----\n");

    //KCB: 1
    // 1.1. Create kcb
    struct capref ram_cap;
    ERROR_RET1(ram_alloc(&ram_cap, OBJSIZE_KCB));
    struct capref kcb;
    ERROR_RET1(slot_alloc(&kcb));
    ERROR_RET1(cap_retype(kcb, ram_cap, 0, ObjType_KernelControlBlock, OBJSIZE_KCB, 1));
    // 1.2. create data_space -> cloned kcb block
    size_t bytes;
    struct capref data_space;
    debug_printf("Frame alloc\n");
    ERROR_RET1(frame_alloc(&data_space, OBJSIZE_KCB, &bytes));
    ERROR_RET1(invoke_kcb_clone(kcb, data_space));
    // 1.3. Map data_space to memory, and access it by core_data variable
    struct arm_core_data* core_data;
    ERROR_RET1(paging_map_frame(get_current_paging_state(),
            (void**)&core_data, OBJSIZE_KCB, data_space, NULL, NULL));
    debug_printf("Core id: %lu, build id: %lu \n", core_data->src_core_id, core_data->build_id);

    struct frame_identity kcb_id;
    ERROR_RET1(frame_identify(kcb, &kcb_id));
    core_data->kcb=kcb_id.base;
    core_data->dst_core_id=1;

    //Init: 2 Load and realocate CPU driver
    // 2.1 Find cpu driver in elf
    struct mem_region* kernel_mem_reg=multiboot_find_module(bi, "cpu_omap44xx");
    if (!kernel_mem_reg)
        return SPAWN_ERR_FIND_MODULE;
    struct capref kernel_frame;
    kernel_frame.cnode = cnode_module;
    kernel_frame.slot = kernel_mem_reg->mrmod_slot;
    // 2.2 Idenitfy kernel frame
    struct frame_identity kernel_frame_id;
    ERROR_RET1(frame_identify(kernel_frame, &kernel_frame_id));
    debug_printf("Kernel frame base address: [0x%08x] bytes: [0x%08x]\n", kernel_frame_id.base, kernel_frame_id.bytes);
    // 2.3 Map frame to memory so it can be reallocated
    void* address;
    ERROR_RET2(paging_map_frame_attr(get_current_paging_state(), &address,
                kernel_frame_id.bytes,
                kernel_frame, VREGION_FLAGS_READ_WRITE,
                NULL, NULL),
                SPAWN_ERR_MAP_MODULE);
    char* elf = (char*)address;
    debug_printf("Kernel elf starts with 0x%x: 0x%x 0x%x 0x%x 0x%x.\n",
        (int)address, elf[0], elf[1], elf[2], elf[3]);

    // 2.4 Allocate new frame for reallocation
    struct capref reallocated_frame_capref;
    ERROR_RET1(frame_alloc(&reallocated_frame_capref, kernel_frame_id.bytes, &bytes));
    void* reallocated_virtual_addr=NULL;
    ERROR_RET1(paging_map_frame(get_current_paging_state(), &reallocated_virtual_addr, bytes,
            reallocated_frame_capref, NULL, NULL));
    struct frame_identity realocation_frame_id;
    ERROR_RET1(frame_identify(reallocated_frame_capref, &realocation_frame_id));

    ERROR_RET1(load_cpu_relocatable_segment(address, reallocated_virtual_addr, realocation_frame_id.base,
            core_data->kernel_load_base, &core_data->got_base));

    struct frame_identity core_data_frame_id;
    ERROR_RET1(frame_identify(data_space, &core_data_frame_id));

    debug_printf("core data frame: 0x[%08x] kernel frame: 0x[%08x]\n", core_data_frame_id.base, kernel_frame_id.base);

    sys_debug_flush_cache();
    sys_armv7_cache_invalidate((void*)((uint32_t)core_data_frame_id.base),
            (void*)((uint32_t)(core_data_frame_id.base+core_data_frame_id.bytes-1)));

    sys_armv7_cache_invalidate((void*)((uint32_t)kcb_id.base),
            (void*)((uint32_t)(kcb_id.base+kcb_id.bytes-1)));

    core_data->cmdline=offsetof(struct arm_core_data, cmdline_buf)+core_data_frame_id.base;

    //Load init
    //Load elf
    struct mem_region* init_mem_region=multiboot_find_module(bi, "init");
    if (!init_mem_region)
        return SPAWN_ERR_FIND_MODULE;
    const lvaddr_t init_memory=ARM_CORE_DATA_PAGES * BASE_PAGE_SIZE*8;

    //Load memory for init
    struct capref init_frame;
    ERROR_RET1(frame_alloc(&init_frame, init_memory, &bytes));
    struct frame_identity init_frame_id;
    ERROR_RET1(frame_identify(init_frame, &init_frame_id));

    struct multiboot_modinfo modinfo={
        .mod_start=init_mem_region->mr_base,
        .mod_end=init_mem_region->mr_base+init_mem_region->mrmod_size,
        .string=init_mem_region->mrmod_data,
        .reserved=0
    };

    strcpy(core_data->init_name, "init");
    core_data->monitor_module=modinfo;
    core_data->memory_base_start=init_frame_id.base;
    core_data->memory_bytes=init_frame_id.bytes;

    ERROR_RET1(frame_alloc(&cap_urpc, BASE_PAGE_SIZE, &bytes));
    struct frame_identity urpc_frame_id;
    ERROR_RET1(frame_identify(cap_urpc, &urpc_frame_id));

    core_data->urpc_frame_base=urpc_frame_id.base;
    core_data->urpc_frame_size=urpc_frame_id.bytes;

    // give other cores a way to access the bootinfo by writing it to the URPC buffer
    void* urpc_buffer;
    ERROR_RET1(paging_map_frame(get_current_paging_state(), &urpc_buffer, urpc_frame_id.bytes, cap_urpc,
                NULL, NULL));

    ERROR_RET1(write_to_urpc(urpc_buffer,urpc_frame_id.base, urpc_frame_id.bytes, bi, 0));

    ERROR_RET1(invoke_monitor_spawn_core(1, CPU_ARM7, core_data_frame_id.base));

    debug_printf("Finished\n");

    return SYS_ERR_OK;

}
