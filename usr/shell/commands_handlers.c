#include <aos/aos_rpc.h>
#include "shell.h"

/**
Commands:
    - [OK] echo
    - [OK] led
    - [OK] threads
    - [OK] memtest
    - oncore
    - [OK] ps
    - [OK] help
*/

static void handle_help(char* const argv[], int argc)
{
    struct command_handler_entry* commands = shell_get_command_table();
    printf("Available commands:\n");
    for (int i = 0; commands[i].name != NULL; ++i)
        printf("\t%s\n", commands[i].name);
}

static void handle_echo(char* const argv[], int argc)
{
    for (int i = 1; i < argc; ++i)
        printf("%s ", argv[i]);
    printf("\n");
}

static void handle_args(char* const argv[], int argc)
{
    debug_printf("Execute command '%s' with %d arguments\n", argv[0], argc);
    for (int i = 1; i < argc; ++i)
        debug_printf("arg[%d]: '%s'\n", i, argv[i]);
}

static void handle_ps(char* const argv[], int argc)
{
    domainid_t* pidptr;
    uint32_t pidcount;
	errval_t err = aos_rpc_process_get_all_pids(get_init_rpc(), &pidptr, &pidcount);
	if (err_is_fail(err)){
		DEBUG_ERR(err, "Could not get PIDs");
		return;
	}
    printf("%u running processes:\n", pidcount);
	char* name;
	for (int i = 0; i < pidcount; i++) {
		err = aos_rpc_process_get_name(get_init_rpc(), pidptr[i], &name);
		if (err_is_ok(err))
		{
			printf("\t%d\t\"%s\"\n", pidptr[i], name);
			free(name);
		}
		else
			DEBUG_ERR(err, "Could not get domain name [pid=%d]\n", pidptr[i]);
	}
	free(pidptr);
}

static int thread_demo(void* tid)
{
    debug_printf("Thread %d is ok\n", *((int*)tid));
    return 0;
}

static void handle_threads(char* const argv[], int argc)
{
    int num_threads = 2;
    if (argc >= 2)
        num_threads = strtol(argv[1], NULL, 10);
    if (num_threads <= 0 || num_threads > 100)
    {
        printf("Invalid number of threads: %i\n", num_threads);
        printf("Syntax: %s $num_threads\n", argv[0]);
        return;
    }
    int* thread_ids = malloc(sizeof(int) * num_threads);
    struct thread** threads = malloc(sizeof(struct thread*) * num_threads);
    debug_printf("Starting %d threads\n", num_threads);
    for (int i = 0; i < num_threads; ++i)
    {
        thread_ids[i] = i;
        threads[i] = thread_create(thread_demo, &thread_ids[i]);
    }
    printf("Joining...\n", num_threads);
    int retval;
    for (int i = 0; i < num_threads; ++i)
        thread_join(threads[i], &retval);
    free(threads);
    free(thread_ids);
}

static void handle_led(char* const argv[], int argc)
{
    int switch_on = -1;
    if (argc == 2 && !strcmp(argv[1], "on"))
        switch_on = 1;
    if (argc == 2 && !strcmp(argv[1], "off"))
        switch_on = 0;
    if (switch_on == -1)
    {
        printf("Syntax: %s {on,off}\n", argv[0]);
        return;
    }
    errval_t err = aos_rpc_set_led(get_init_rpc(), switch_on);
    if (err_is_ok(err))
    {
        printf("LED %sabled\n", switch_on ? "en" : "dis");
        return;
    }
    DEBUG_ERR(err, "Error switching led status");
}

static void handle_memtest(char* const argv[], int argc)
{
    printf("** DISCLAIMER **\n");
    printf("strtol doesnt support values above 1<<31!\n");
    if (argc != 3)
    {
        printf("Syntax: %s base_address size\n", argv[0]);
        return;
    }
    lpaddr_t base = strtol(argv[1], NULL, 0);
    size_t size = strtol(argv[2], NULL, 0);
    printf("Testing memory from 0x%08x to 0x%08x [size = 0x%08x]...\n",
        (unsigned int)base, (unsigned int)(base + size), (unsigned int)size);
    errval_t err = aos_rpc_memtest(get_init_rpc(), base, size);
    if (err_is_ok(err))
    {
        printf("Test finished\n");
        return;
    }
    DEBUG_ERR(err, "Error in memory test");
}

bool shell_execute_command(char* const argv[], int argc)
{
    struct command_handler_entry* commands = shell_get_command_table();
    for (int i = 0; commands[i].name != NULL; ++i)
        if (!strcmp(argv[0], commands[i].name))
        {
            commands[i].handler(argv, argc);
            return true;
        }
    return false;
}

struct command_handler_entry* shell_get_command_table(void)
{
    static struct command_handler_entry commandsTable[] = {
        {.name = "args",        .handler = handle_args},
        {.name = "echo",        .handler = handle_echo},
        {.name = "help",        .handler = handle_help},
        {.name = "led",         .handler = handle_led},
        {.name = "memtest",     .handler = handle_memtest},
        {.name = "ps",          .handler = handle_ps},
        {.name = "threads",     .handler = handle_threads},
        {.name = NULL}
    };
    return commandsTable;
}
