/**
 * \file
 * \brief Local memory allocator for init till mem_serv is ready to use
 */

#include "mem_alloc.h"
#include <mm/mm.h>

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
    errval_t err = mm_mem_init();
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

void test_alloc(size_t size, size_t alignment, struct capref* ref);
void test_alloc(size_t size, size_t alignment, struct capref* ref)
{
	struct mm* mm = mm_get_default();
	errval_t err;
	if (alignment)
		err = mm_alloc_aligned(mm, size, alignment, ref);
	else
		err = mm_alloc(mm, size, ref);
	MM_ASSERT(err, "Alloc failed");
	debug_printf("\tAllocated cap [0x%x] at slot %u.\n", get_cap_addr(*ref), ref->slot);
}
