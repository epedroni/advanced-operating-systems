#include <stdio.h>
#include <aos/aos.h>
#include <spawn/spawn.h>
#include <aos/kernel_cap_invocations.h>
#include <target/arm/barrelfish_kpi/arm_core_data.h>
#include <spawn/spawn.h>
#include <spawn/multiboot.h>
#include <aos/coreboot.h>
#include <stddef.h>

struct coreboot_available_ram_info{
    genpaddr_t ram_base_address;
    genpaddr_t ram_size;
};

struct urpc_buffer_header{
    struct coreboot_available_ram_info ram_info;
    volatile uint32_t spawned_core_id;
};

errval_t coreboot_init(struct bootinfo *bi, void** urpc_buffer, size_t* urpc_buffer_size);
errval_t coreboot_read_bootinfo_from_urpc(void* urpc_buf, struct bootinfo** bi, struct coreboot_available_ram_info* available_ram);
errval_t coreboot_write_bootinfo_to_urpc(void* urpc_buf, genpaddr_t base, gensize_t size, struct bootinfo* bi,
        coreid_t core_to_spawn_on, struct coreboot_available_ram_info available_ram);
errval_t coreboot_urpc_read_bootinfo_modules(void* urpc_buf, struct bootinfo* bi);
errval_t coreboot_wait_for_core_to_boot(void* urpc_buf);
errval_t coreboot_finished_init(void* urpc_buf);
