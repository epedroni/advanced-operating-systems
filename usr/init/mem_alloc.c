/**
 * \file
 * \brief Local memory allocator for init till mem_serv is ready to use
 */

#include "mem_alloc.h"
#include <mm/mm.h>
#include <aos/paging.h>

/// MM allocator instance data
struct mm aos_mm;

void runtests_mem_alloc(void);

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
errval_t initialize_ram_alloc(void)
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
	err = mm_init(&aos_mm, ObjType_RAM, NULL,
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

	// Finally, we can initialize the generic RAM allocator to use our local allocator
	err = ram_alloc_set(aos_ram_alloc_aligned);
	if (err_is_fail(err)) {
		return err_push(err, LIB_ERR_RAM_ALLOC_SET);
	}

	runtests_mem_alloc();

	return SYS_ERR_OK;
}

void test_alloc(size_t size, size_t alignment, struct capref* ref);
void test_alloc(size_t size, size_t alignment, struct capref* ref)
{
	errval_t err;
	if (alignment)
		err = mm_alloc_aligned(&aos_mm, size, alignment, ref);
	else
		err = mm_alloc(&aos_mm, size, ref);
	MM_ASSERT(err, "Alloc failed");
	debug_printf("\tAllocated cap [0x%x] at slot %u.\n", get_cap_addr(*ref), ref->slot);
}

void runtests_mem_alloc(void)
{
	debug_printf("Running mem_alloc tests set\n");

	mm_print_nodes(&aos_mm);

	// Keep it simple for now, allocate 4kB caps
	int alloc_size = BASE_PAGE_SIZE;
	debug_printf("Allocate 5x%u bits...\n", alloc_size);
	struct capref smallCaps[5];
	for (int i = 0; i < 5; ++i)
	{
		errval_t err = mm_alloc(&aos_mm, alloc_size, &smallCaps[i]);
		MM_ASSERT(err, "Alloc failed");
		debug_printf("\tAllocated cap [0x%x] at slot %u. Ret %u\n", &smallCaps[i], smallCaps[i].slot, err);
	}

	// Test large pages now - 1MB
	struct capref largeCaps[5];
	alloc_size = LARGE_PAGE_SIZE;
	debug_printf("Allocate 5x%u bits...\n", alloc_size);
	for (int i = 0; i < 5; ++i)
	{
		errval_t err = mm_alloc(&aos_mm, alloc_size, &largeCaps[i]);
		MM_ASSERT(err, "Alloc failed");
		debug_printf("\tAllocated cap [0x%x] at slot %u. Ret %u\n", &largeCaps[i], largeCaps[i].slot, err);
	}

	// Let's test merging now - if we free the original 5 caps and try to allocate something like 16kB, those nodes should be merged and used
	alloc_size = BASE_PAGE_SIZE;
	debug_printf("Freeing 5x%u bits...\n", alloc_size);
	for (int i = 0; i < 5; ++i)
	{
		debug_printf("\tFreeing alloc #%u\n", i);
		MM_ASSERT(aos_ram_free(smallCaps[i], alloc_size), "mm_free failed");
	}

	struct capref mediumCap;
	alloc_size = BASE_PAGE_SIZE * 4;
	debug_printf("Allocate %u bits...\n", alloc_size);
	errval_t err = mm_alloc(&aos_mm, alloc_size, &mediumCap);
	MM_ASSERT(err, "Alloc failed");
	debug_printf("\tAllocated cap [0x%x] at slot %u. Ret %u\n", &mediumCap, mediumCap.slot, err);

	// Now free everything and check if we are back to the single ram chunk again
	alloc_size = LARGE_PAGE_SIZE;
	debug_printf("Freeing 5x%u bits...\n", alloc_size);
	for (int i = 0; i < 5; ++i)
	{
		debug_printf("\tFreeing alloc #%u\n", i);
		MM_ASSERT(aos_ram_free(largeCaps[i], alloc_size), "mm_free failed");
		debug_printf("Done\n");
	}

	alloc_size = BASE_PAGE_SIZE * 4;
	debug_printf("\tFreeing medium-sized alloc\n");
	MM_ASSERT(aos_ram_free(mediumCap, alloc_size), "mm_free failed");

	mm_print_nodes(&aos_mm);

//	struct capref cap[10];
//	for (int j = 0; j < 10; ++j)
//	{
//		int alloc_size = BASE_PAGE_SIZE;
//		debug_printf("[%u] Allocate 10x%ubits...\n", j, alloc_size);
//		for (int i = 0; i < 10; ++i)
//			test_alloc(alloc_size, 0, &cap[i]);
//
//		debug_printf("[%u] Freeing 10x%ubits...\n", j, alloc_size);
//		for (int i = 0; i < 10; ++i)
//		{
//			debug_printf("\tFreeing alloc #%u\n", i);
//			MM_ASSERT(aos_ram_free(cap[i], alloc_size), "mm_free failed");
//		}
//	}
//	debug_printf("Allocate aligned...\n");
//	for (int i = 1; i < 10; ++i)
//		test_alloc(i*BASE_PAGE_SIZE-42, (i+1)*BASE_PAGE_SIZE, &cap[i]);
//	debug_printf("Freeing memory...\n");
//	for (int i = 1; i < 10; ++i)
//		MM_ASSERT(aos_ram_free(cap[i], i*BASE_PAGE_SIZE-42), "mm_free failed");

	//    struct capref cap[10];
	//    for (int j = 0; j < 10; ++j)
	//    {
	//        int alloc_size = BASE_PAGE_SIZE;
	//        debug_printf("[%u] Allocate 10x%ubits...\n", j, alloc_size);
	//        for (int i = 0; i < 10; ++i)
	//            test_alloc(alloc_size, 0, &cap[i]);
	//
	//        debug_printf("[%u] Freeing 10x%ubits...\n", j, alloc_size);
	//        for (int i = 0; i < 10; ++i)
	//        {
	//            debug_printf("\tFreeing alloc #%u\n", i);
	//            MM_ASSERT(aos_ram_free(cap[i], alloc_size), "mm_free failed");
	//        }
	//    }
	//    debug_printf("Allocate aligned...\n");
	//    for (int i = 1; i < 10; ++i)
	//        test_alloc(i*BASE_PAGE_SIZE-42, (i+1)*BASE_PAGE_SIZE, &cap[i]);
	//    debug_printf("Freeing memory...\n");
	//    for (int i = 1; i < 10; ++i)
	//        MM_ASSERT(aos_ram_free(cap[i], i*BASE_PAGE_SIZE-42), "mm_free failed");
}
