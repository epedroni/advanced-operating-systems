/**
 * \file
 * \brief Local memory allocator for init till mem_serv is ready to use
 */

#include "mem_alloc.h"
#include <mm/mm.h>
#include <aos/threads.h>

static
errval_t aos_slab_refill(struct slab_allocator *slabs){
	static int refill = 0;

    SLAB_DEBUG_OUT("[0x%08x:%s] aos_slab_refill",
        (int)slabs, slabs->name);
	if (refill)
		return SYS_ERR_OK;

	++refill;
	assert(refill == 1 && "Most likely race cond in aos_slab_refill!!");

	// TODO: To be changed once we have a correct malloc.
	void* memory;
	errval_t err = malloc_pages(&memory, 1);
	if (err_is_fail(err))
	{
		--refill;
		return err;
	}

	slab_grow(slabs, memory, BASE_PAGE_SIZE);
	--refill;

	return SYS_ERR_OK;
}

static errval_t aos_ram_alloc_aligned(struct capref *ret, size_t size, size_t alignment)
{
	return mm_alloc_aligned(&aos_mm, size, alignment, ret);
}

errval_t aos_ram_free(struct capref cap, size_t bytes)
{
	errval_t err;
	struct frame_identity fi;
	err = frame_identify(cap, &fi);
	if (bytes > fi.bytes) {
		bytes = fi.bytes;
	}
	return mm_free(&aos_mm, cap, fi.base, bytes);
}

/**
 * \brief Setups a local memory allocator for init to use till the memory server
 * is ready to be used.
 */
errval_t initialize_ram_alloc(coreid_t core_id, genpaddr_t ram_base_address, genpaddr_t ram_size)
{
    printf("Initializing RAM allocator ...\n");
    errval_t err = aos_init_mm(core_id, ram_base_address, ram_size);
    if (err_is_fail(err)) {
        return err;
    }

    // Finally, we can initialize the generic RAM allocator to use our local allocator
    debug_printf("aos ram alloc\n");
    err = ram_alloc_set(aos_ram_alloc_aligned);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_RAM_ALLOC_SET);
    }
    debug_printf("Done initialize ram alloc\n");
    return err;
}

/**
 * \brief Setups a local memory allocator for init to use till the memory server
 * is ready to be used.
 */
errval_t aos_init_mm(coreid_t core_id, genpaddr_t ram_base_address, genpaddr_t ram_size)
{
    debug_printf("aos init mm, core id: %lu\n", core_id);
    errval_t err;

    // Init slot allocator
    static struct slot_prealloc init_slot_alloc;
    struct capref cnode_cap = {
        .cnode = {
            .croot = CPTR_ROOTCN,
            .cnode = ROOTCN_SLOT_ADDR(ROOTCN_SLOT_SLOT_ALLOC0),
            .level = CNODE_TYPE_OTHER,
        },
        .slot = 0,
    };
    debug_printf("invoking: slot_prealloc_init\n");
    err = slot_prealloc_init(&init_slot_alloc, cnode_cap, L2_CNODE_SLOTS, &aos_mm);
    if (err_is_fail(err)) {
        return err_push(err, MM_ERR_SLOT_ALLOC_INIT);
    }

    debug_printf("invoking mm_init\n");
    // Initialize aos_mm
    err = mm_init(&aos_mm, ObjType_RAM, aos_slab_refill,
                  slot_alloc_prealloc, slot_prealloc_refill,
                  &init_slot_alloc);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "Can't initalize the memory manager.");
    }

    // Give aos_mm a bit of memory for the initialization
    debug_printf("invoking slab grow\n");
    static char nodebuf[sizeof(struct mmnode)*64];
    slab_grow(&aos_mm.slabs, nodebuf, sizeof(nodebuf));

    // Walk bootinfo and add all RAM caps to allocator handed to us by the kernel
    uint64_t mem_avail = 0;
    struct capref mem_cap = {
        .cnode = cnode_super,
        .slot = 0,
    };

    size_t ram_cap_index=0;
    debug_printf("Bad address is: [0x%08x]\n", bi);
    if(core_id==0){
        for (int i = 0; i < bi->regions_length; i++) {
            if (bi->regions[i].mr_type == RegionType_Empty) {
                if(ram_cap_index++!=core_id){
                    continue;
                }

                debug_printf("Adding region of ram %lu\n",i);
                err = mm_add(&aos_mm, mem_cap, bi->regions[i].mr_base, bi->regions[i].mr_bytes);
                if (err_is_ok(err)) {
                    mem_avail += bi->regions[i].mr_bytes;
                } else {
                    DEBUG_ERR(err, "Warning: adding RAM region %d (%p/%zu) FAILED", i, bi->regions[i].mr_base, bi->regions[i].mr_bytes);
                }

                err = slot_prealloc_refill(aos_mm.slot_alloc_inst);
                if (err_is_fail(err) && err_no(err) != MM_ERR_SLOT_MM_ALLOC) {
                    DEBUG_ERR(err, "in slot_prealloc_refill() while initialising"
                            " memory allocator");
                    abort();
                }

                mem_cap.slot++;
            }
        }
        debug_printf("Added %"PRIu64" MB of physical memory.\n", mem_avail / 1024 / 1024);
    }else{
        debug_printf("Forging a capability!\n");

        struct capref forged_ram;
        ERROR_RET1(slot_alloc(&forged_ram));
        ERROR_RET1(ram_forge(forged_ram, ram_base_address, ram_size, core_id));
        ERROR_RET1(mm_add(&aos_mm,forged_ram, ram_base_address, ram_size));

        err = slot_prealloc_refill(aos_mm.slot_alloc_inst);
        if (err_is_fail(err) && err_no(err) != MM_ERR_SLOT_MM_ALLOC) {
            DEBUG_ERR(err, "in slot_prealloc_refill() while initialising"
                    " memory allocator");
            abort();
        }
    }

    return SYS_ERR_OK;
}
