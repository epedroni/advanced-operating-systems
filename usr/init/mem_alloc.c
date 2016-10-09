/**
 * \file
 * \brief Local memory allocator for init till mem_serv is ready to use
 */

#include "mem_alloc.h"
#include <mm/mm.h>

errval_t aos_slab_refill(struct slab_allocator *slabs){
	debug_printf("AOS SLAB REFILL FUNCTION \n");

	static bool refill = false;
	if (refill){
		return SYS_ERR_OK;
	}

	refill = true;
	// TODO: To be changed once we have a correct malloc.
	size_t allocated_size;
	void* memory = get_mapped_page(&allocated_size);

	slab_grow(slabs, memory, allocated_size);
	refill = false;

	return SYS_ERR_OK;
}

static errval_t aos_ram_alloc_aligned(struct capref *ret, size_t size, size_t alignment)
{
	return mm_alloc_aligned(mm_get_default(), size, alignment, ret);
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
errval_t initialize_ram_alloc(void)
{
    errval_t err = aos_init_mm();
    if (err_is_fail(err)) {
        return err;
    }

    // Finally, we can initialize the generic RAM allocator to use our local allocator
    err = ram_alloc_set(aos_ram_alloc_aligned);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_RAM_ALLOC_SET);
    }
	return err;
}

//void test_alloc(size_t size, size_t alignment, struct capref* ref)
//{
//	struct mm* mm = mm_get_default();
//	errval_t err;
//	if (alignment)
//		err = mm_alloc_aligned(mm, size, alignment, ref);
//	else
//		err = mm_alloc(mm, size, ref);
//	MM_ASSERT(err, "Alloc failed");
//	debug_printf("\tAllocated cap [0x%x] at slot %u.\n", get_cap_addr(*ref), ref->slot);
//}

/**
 * \brief Setups a local memory allocator for init to use till the memory server
 * is ready to be used.
 */
errval_t aos_init_mm(void)
{
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
    err = slot_prealloc_init(&init_slot_alloc, cnode_cap, L2_CNODE_SLOTS, &aos_mm);
    if (err_is_fail(err)) {
        return err_push(err, MM_ERR_SLOT_ALLOC_INIT);
    }

    // Initialize aos_mm
    err = mm_init(&aos_mm, ObjType_RAM, aos_slab_refill,
                  slot_alloc_prealloc, slot_prealloc_refill,
                  &init_slot_alloc);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "Can't initalize the memory manager.");
    }

    // Give aos_mm a bit of memory for the initialization
    static char nodebuf[sizeof(struct mmnode)*64];
    slab_grow(&aos_mm.slabs, nodebuf, sizeof(nodebuf));

    // Walk bootinfo and add all RAM caps to allocator handed to us by the kernel
    uint64_t mem_avail = 0;
    struct capref mem_cap = {
        .cnode = cnode_super,
        .slot = 0,
    };

    for (int i = 0; i < bi->regions_length; i++) {
        if (bi->regions[i].mr_type == RegionType_Empty) {
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

    return SYS_ERR_OK;
}

void* get_mapped_page(size_t* alloc_size) {
	*alloc_size=BASE_PAGE_SIZE;

	struct paging_state* paging_state =get_current_paging_state();

    struct capref cap_ram;
    debug_printf("mm_alloc invoking\n");
    errval_t err = mm_alloc(&aos_mm, *alloc_size, &cap_ram);
    MM_ASSERT(err, "test_paging: ram_alloc_fixed");

    struct capref cap_as_frame;
    debug_printf("Allocating slot\n");
	err = paging_state->slot_alloc->alloc(paging_state->slot_alloc, &cap_as_frame);
	err = cap_retype(cap_as_frame, cap_ram, 0,
            ObjType_Frame, *alloc_size, 1);
    MM_ASSERT(err, "test_paging: cap_retype");

    void* address=NULL;
    debug_printf("Map frame attr\n");
    err=paging_map_frame_attr(paging_state,&address, *alloc_size,
    		cap_as_frame,VREGION_FLAGS_READ_WRITE,NULL,NULL);
    MM_ASSERT(err, "get_page: paging_map_fixed_attr");

	return address;
}
