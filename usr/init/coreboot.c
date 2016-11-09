#include "coreboot.h"

#define KERNEL_WINDOW 0x80000000

errval_t coreboot_init(void){
    debug_printf("---- starting coreboot init ----\n");

    struct capref ram_cap;
    ERROR_RET1(ram_alloc(&ram_cap, OBJSIZE_KCB));

    debug_printf("output\n");
    struct capref kcb;
    ERROR_RET1(slot_alloc(&kcb));
    ERROR_RET1(cap_retype(kcb, ram_cap, 0, ObjType_KernelControlBlock, OBJSIZE_KCB, 1));

    size_t bytes;
    struct capref data_space;
    debug_printf("Frame alloc\n");
    ERROR_RET1(frame_alloc(&data_space, OBJSIZE_KCB, &bytes));
    ERROR_RET1(invoke_kcb_clone(kcb, data_space));

    debug_printf("map page frame\n");
    struct arm_core_data* core_data;
    ERROR_RET1(paging_map_frame(get_current_paging_state(),
            (void**)&core_data, OBJSIZE_KCB, data_space, NULL, NULL));
    debug_printf("Core id: %lu, build id: %lu \n", core_data->src_core_id, core_data->build_id);

    //Allocate space for init

    debug_printf("Finding init\n");
    struct spawninfo init_si;
    struct mem_region* init_mem_region;
    ERROR_RET1(spawn_load_module(&init_si, "init", &init_mem_region));
    debug_printf("Allocating init space\n");
    void* init_memory=NULL;
    init_memory=malloc(ARM_CORE_DATA_PAGES * BASE_PAGE_SIZE);
    assert(init_memory);

    debug_printf("Filling core data\n");
    core_data->monitor_module.mod_start=(uint32_t)init_mem_region;
    core_data->monitor_module.mod_end=(uint32_t)init_mem_region+init_si.module_bytes;
    core_data->monitor_module.string=(uint32_t)init_mem_region->mrmod_data;
    core_data->monitor_module.reserved=(uint32_t)0;
    core_data->memory_base_start=(uint32_t)init_memory;
    core_data->memory_bytes=(uint32_t)ARM_CORE_DATA_PAGES * BASE_PAGE_SIZE;
    core_data->cmdline=(lvaddr_t)core_data->cmdline_buf;

    debug_printf("Loading cpu driver\n");
    struct spawninfo cpu_driver_si;
    struct mem_region* cpu_driver_mem_reg;
    ERROR_RET1(spawn_load_module(&cpu_driver_si, "cpu_omap44xx", &cpu_driver_mem_reg));
    struct frame_identity cpu_driver_info;
    ERROR_RET1(frame_identify(cpu_driver_si.module_frame, &cpu_driver_info));

    void* elf_address=NULL;
    paging_map_frame(get_current_paging_state(), &elf_address, cpu_driver_info.bytes,
            cpu_driver_si.module_frame, NULL, NULL);

    struct capref reallocated_frame_capref;
    void* reallocated_virtual_addr=NULL;
    ERROR_RET1(frame_alloc(&reallocated_frame_capref, cpu_driver_info.bytes,&bytes));
    ERROR_RET1(paging_map_frame(get_current_paging_state(),&reallocated_virtual_addr, bytes,reallocated_frame_capref,NULL,NULL));
    struct frame_identity reallocated_id;
    ERROR_RET1(frame_identify(reallocated_frame_capref, &reallocated_id));

    load_cpu_relocatable_segment(cpu_driver_mem_reg, reallocated_virtual_addr, reallocated_id.base+KERNEL_WINDOW,
             core_data->kernel_load_base, &core_data->got_base);

    return SYS_ERR_OK;

}
