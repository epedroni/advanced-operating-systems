#include "coreboot.h"

#define KERNEL_WINDOW 0x80000000

extern struct capref cap_urpc;

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
    frame_identify(kcb, &kcb_id);
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
    frame_identify(kernel_frame, &kernel_frame_id);
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
    frame_identify(reallocated_frame_capref, &realocation_frame_id);

    ERROR_RET1(load_cpu_relocatable_segment(address, reallocated_virtual_addr, realocation_frame_id.base,
            core_data->kernel_load_base, &core_data->got_base));

    struct frame_identity core_data_frame_id;
    frame_identify(data_space, &core_data_frame_id);

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
    frame_alloc(&init_frame, init_memory, &bytes);
    struct frame_identity init_frame_id;
    frame_identify(init_frame, &init_frame_id);

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

    //struct capref urpc_frame;
    frame_alloc(&cap_urpc, BASE_PAGE_SIZE, &bytes);
    struct frame_identity urpc_frame_id;
    frame_identify(cap_urpc, &urpc_frame_id);

    core_data->urpc_frame_base=urpc_frame_id.base;
    core_data->urpc_frame_size=urpc_frame_id.bytes;

//    void* urpc_data;
//    paging_map_frame(get_current_paging_state(), &urpc_data, BASE_PAGE_SIZE,
//                urpc_frame, NULL, NULL);

    ERROR_RET1(invoke_monitor_spawn_core(1, CPU_ARM7, core_data_frame_id.base));

    debug_printf("Finished\n");

//    debug_printf("Finding init\n");
//    struct spawninfo init_si;
//    struct mem_region* init_mem_region;
//    ERROR_RET1(spawn_load_module(&init_si, "init", &init_mem_region));
//    debug_printf("Allocating init space\n");
//    void* init_memory=NULL;
//    init_memory=malloc(ARM_CORE_DATA_PAGES * BASE_PAGE_SIZE);
//    assert(init_memory);
//
//    debug_printf("Creating URPC frame\n");
//    size_t urpc_frame_size = BASE_PAGE_SIZE;
//    void *urpc_frame = malloc(urpc_frame_size);
//
//    debug_printf("Filling core data\n");
//    core_data->monitor_module.mod_start=(uint32_t)init_mem_region;
//    core_data->monitor_module.mod_end=(uint32_t)init_mem_region+init_si.module_bytes;
//    core_data->monitor_module.string=(uint32_t)init_mem_region->mrmod_data;
//    core_data->monitor_module.reserved=(uint32_t)0;
//    core_data->memory_base_start=(uint32_t)init_memory;
//    core_data->memory_bytes=(uint32_t)ARM_CORE_DATA_PAGES * BASE_PAGE_SIZE;
//    core_data->cmdline=(lvaddr_t)core_data->cmdline_buf;
//    core_data->urpc_frame_base = (uint32_t)urpc_frame;
//    core_data->urpc_frame_size = urpc_frame_size;
//
//    debug_printf("Loading cpu driver\n");
//    struct spawninfo cpu_driver_si;
//    struct mem_region* cpu_driver_mem_reg;
//    ERROR_RET1(spawn_load_module(&cpu_driver_si, "cpu_omap44xx", &cpu_driver_mem_reg));
//    struct frame_identity cpu_driver_info;
//    ERROR_RET1(frame_identify(cpu_driver_si.module_frame, &cpu_driver_info));
//
//    void* elf_address=NULL;
//    paging_map_frame(get_current_paging_state(), &elf_address, cpu_driver_info.bytes,
//            cpu_driver_si.module_frame, NULL, NULL);
//
//    struct capref reallocated_frame_capref;
//    void* reallocated_virtual_addr=NULL;
//    ERROR_RET1(frame_alloc(&reallocated_frame_capref, cpu_driver_info.bytes,&bytes));
//    ERROR_RET1(paging_map_frame(get_current_paging_state(), &reallocated_virtual_addr, bytes,reallocated_frame_capref,NULL,NULL));
//    struct frame_identity reallocated_id;
//    ERROR_RET1(frame_identify(reallocated_frame_capref, &reallocated_id));
//
//    load_cpu_relocatable_segment(cpu_driver_mem_reg, reallocated_virtual_addr, reallocated_id.base+KERNEL_WINDOW,
//             core_data->kernel_load_base, &core_data->got_base);
//
//    debug_printf("Flushing caches and stuff\n");
//    debug_printf("Alleged entry point: 0x%x\n", core_data->entry_point);
//
//    sys_debug_flush_cache();
//    sys_armv7_cache_invalidate();
//
//    debug_printf("Kernel l1 high: 0x[%08x] l1 low: 0x[%08x]\n",
//            core_data->kernel_l1_high, core_data->kernel_l1_low);
//    invoke_monitor_spawn_core(1, CPU_ARM7, core_data->entry_point);

    return SYS_ERR_OK;

}
