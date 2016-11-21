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

errval_t coreboot_init(struct bootinfo *bi);
errval_t coreboot_read_bootinfo_from_urpc(void* urpc_buf, struct bootinfo** bi, struct coreboot_available_ram_info* available_ram,
        coreid_t my_core_id);
errval_t coreboot_write_bootinfo_to_urpc(void* urpc_buf, genpaddr_t base, gensize_t size, struct bootinfo* bi, coreid_t my_core_id,
        struct coreboot_available_ram_info available_ram);
errval_t coreboot_urpc_read_bootinfo_modules(void* urpc_buf, struct bootinfo* bi, coreid_t my_core_id);
