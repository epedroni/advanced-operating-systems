#include <aos/aos.h>

#include "mem_alloc.h"
#include "tests.h"

//#define VERBOSE_TEST

#define TEST_ASSERT(call, msg) { errval_t _err = (call); if (err_is_fail(_err)) USER_PANIC_ERR(_err, msg);}

#ifdef VERBOSE_TEST
    #define TEST_PRINTF(...) debug_printf(__VA_ARGS__)
#else
    #define TEST_PRINTF(...)
#endif

static uint32_t test_num;

void test_allocate_frame(size_t alloc_size, struct capref* cap_as_frame);
void* test_alloc_and_map(size_t alloc_size);
void runtests_mem_alloc(void);
void test_paging(void);

void run_all_tests(void)
{
    debug_printf("[TEST] Running all tests...\n");
    test_num = 0;
    runtests_mem_alloc();
    test_paging();
    debug_printf("[TEST] Tests finished\n");
}


void runtests_mem_alloc(void)
{
    TEST_PRINTF("Running mem_alloc tests set\n");

    #define TEST_ALLOC(size, cap) {\
        TEST_ASSERT(ram_alloc(&cap, size), "Alloc failed");\
    }

    struct capref ref;
    TEST_PRINTF("\tAllocating a LOT of small pages\n");
    for (int i = 0; i < 500; ++i)
        TEST_ALLOC(BASE_PAGE_SIZE, ref);


    // Keep it simple for now, allocate 4kB caps
    int alloc_size = BASE_PAGE_SIZE;
    TEST_PRINTF("Allocate 5x%u bits...\n", alloc_size);
    struct capref smallCaps[5];
    for (int i = 0; i < 5; ++i)
       TEST_ALLOC(alloc_size, smallCaps[i]);

    // Test large pages now - 1MB
    struct capref largeCaps[5];
    alloc_size = LARGE_PAGE_SIZE;
    TEST_PRINTF("Allocate 5x%u bits...\n", alloc_size);
    for (int i = 0; i < 5; ++i)
        TEST_ALLOC(alloc_size, largeCaps[i]);

    // Let's test merging now - if we free the original 5 caps and try to allocate something like 16kB, those nodes should be merged and used
    alloc_size = BASE_PAGE_SIZE;
    TEST_PRINTF("Freeing 5x%u bits...\n", alloc_size);
    for (int i = 0; i < 5; ++i)
        TEST_ASSERT(aos_ram_free(smallCaps[i], alloc_size), "aos_ram_free");

    struct capref mediumCap;
    alloc_size = BASE_PAGE_SIZE * 4;
    TEST_PRINTF("Allocate 1x%u bits...\n", alloc_size);
    TEST_ALLOC(alloc_size, mediumCap)

    // Now free everything and check if we are back to the single ram chunk again
    alloc_size = LARGE_PAGE_SIZE;
    TEST_PRINTF("Freeing 5x%u bits...\n", alloc_size);
    for (int i = 0; i < 5; ++i)
        TEST_ASSERT(aos_ram_free(largeCaps[i], alloc_size), "mm_free failed");

    alloc_size = BASE_PAGE_SIZE * 4;
    TEST_PRINTF("\tFreeing medium-sized alloc\n");
    TEST_ASSERT(aos_ram_free(mediumCap, alloc_size), "mm_free failed");
}

void test_allocate_frame(size_t alloc_size, struct capref* cap_as_frame) {
    struct paging_state* paging_state =get_current_paging_state();

    struct capref cap_ram;
    TEST_PRINTF("test_allocate_frame: Allocating RAM...\n");
    errval_t err = ram_alloc(&cap_ram, alloc_size);
    TEST_ASSERT(err, "test_allocate_frame: ram_alloc_fixed");

    err = paging_state->slot_alloc->alloc(paging_state->slot_alloc, cap_as_frame);
    err = cap_retype(*cap_as_frame, cap_ram, 0,
            ObjType_Frame, alloc_size, 1);
    TEST_ASSERT(err, "test_allocate_frame: cap_retype");
}

void* test_alloc_and_map(size_t alloc_size) {
    struct paging_state* paging_state =get_current_paging_state();
    struct capref cap_as_frame;
    test_allocate_frame(alloc_size, &cap_as_frame);

    void* address=NULL;
    errval_t err = paging_map_frame_attr(paging_state,&address,alloc_size,
            cap_as_frame,VREGION_FLAGS_READ_WRITE,NULL,NULL);
    TEST_ASSERT(err, "get_page: paging_map_fixed_attr");

    return address;
}

void test_paging(void)
{
    #define PRINT_TEST(title) TEST_PRINTF("###########################\n"); TEST_PRINTF("#TEST%02u: %s\n", ++test_num, title);

    int* number;


    PRINT_TEST("Allocate and map one page");
    void* page = test_alloc_and_map(BASE_PAGE_SIZE);
    number = (int*)page;
    *number=42;

    PRINT_TEST("Allocate and map 2 pages");
    page = test_alloc_and_map(2 * BASE_PAGE_SIZE);
    number = (int*)page;
    for (int i = 0; i < (2 * BASE_PAGE_SIZE) / sizeof(int); ++i)
       number[i] = i;

    PRINT_TEST("Allocate pages to check allocater reuses l2 page table correctly");
    int *numbers[5];

    int i=0;
    for (;i<5;i++){
        void* page1=test_alloc_and_map(BASE_PAGE_SIZE);
        numbers[i]=(int*)page1;
        *numbers[i]=42+i;
    }

    for (i=0; i<5; i++){
        TEST_PRINTF("%d\n",*numbers[i]);
    }

    PRINT_TEST("Allocate big big page (over several L2)");
    number = (int*)test_alloc_and_map(5 * LARGE_PAGE_SIZE);
    for (i = 0; i < 5*LARGE_PAGE_SIZE / sizeof(int); i+=BASE_PAGE_SIZE)
        number[i] = 42+i;

    for (i = 0; i < 5*LARGE_PAGE_SIZE / sizeof(int); i+=BASE_PAGE_SIZE)
        if (i+42 != number[i])
              TEST_PRINTF("Reading byte from page: %d it should be %d and it is: %d\n", i+1, i+42, number[i]);

    PRINT_TEST("Unmap test.");
    number = (int*)test_alloc_and_map(BASE_PAGE_SIZE);
    *number = 42;
    errval_t err = paging_unmap(get_current_paging_state(), (void*)number);
    TEST_ASSERT(err, "unmap");
    // Un-comment the 2 following lines to test.
    // But kind of destructive test :p
    //TEST_PRINTF("Should crash now\n");
    //*number = 1;
}
