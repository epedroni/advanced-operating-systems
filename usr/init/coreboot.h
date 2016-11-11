#include <stdio.h>
#include <aos/aos.h>
#include <spawn/spawn.h>
#include <aos/kernel_cap_invocations.h>
#include <target/arm/barrelfish_kpi/arm_core_data.h>
#include <spawn/spawn.h>
#include <spawn/multiboot.h>
#include <aos/coreboot.h>
#include <stddef.h>

errval_t coreboot_init(struct bootinfo *bi);
errval_t read_from_urpc(void* urpc_buf, struct bootinfo** bi,coreid_t my_core_id);
void write_to_urpc(void* urpc_buf, genpaddr_t base, gensize_t size, struct bootinfo* bi, coreid_t my_core_id);
errval_t read_modules(void* urpc_buf, struct bootinfo* bi, coreid_t my_core_id);
