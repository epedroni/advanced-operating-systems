#include <aos/aos_rpc.h>
#include <fs/fs.h>
#include <fs/dirent.h>

#include "shell.h"

#define SHELL_STDOUT(...) fprintf(shell_get_state()->out, __VA_ARGS__)
#define SHELL_STDERR(...) fprintf(shell_get_state()->err, __VA_ARGS__)
/**
Commands:
    - [OK] echo
    - [OK] led
    - [OK] threads
    - [OK] memtest
    - [OK] oncore
    - [OK] ps
    - [OK] help
*/

static void handle_help(char* const argv[], int argc)
{
    struct command_handler_entry* commands = shell_get_command_table();
    SHELL_STDOUT("Available commands:\n");
    for (int i = 0; commands[i].name != NULL; ++i)
        SHELL_STDOUT("\t%s\n", commands[i].name);
}

static void handle_echo(char* const argv[], int argc)
{
    for (int i = 1; i < argc; ++i)
        SHELL_STDOUT("%s ", argv[i]);
    SHELL_STDOUT("\n");
}

static void handle_args(char* const argv[], int argc)
{
    SHELL_STDOUT("Execute command '%s' with %d arguments\n", argv[0], argc);
    for (int i = 1; i < argc; ++i)
        SHELL_STDOUT("arg[%d]: '%s'\n", i, argv[i]);
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
    SHELL_STDOUT("%u running processes:\n", pidcount);
	char* name;
	for (int i = 0; i < pidcount; i++) {
		err = aos_rpc_process_get_name(get_init_rpc(), pidptr[i], &name);
		if (err_is_ok(err))
		{
			SHELL_STDOUT("\t%d\t\"%s\"\n", pidptr[i], name);
			free(name);
		}
		else
			DEBUG_ERR(err, "Could not get domain name [pid=%d]\n", pidptr[i]);
	}
	free(pidptr);
}

static int thread_demo(void* tid)
{
    SHELL_STDOUT("Thread %d is ok\n", *((int*)tid));
    return 0;
}

static void handle_threads(char* const argv[], int argc)
{
    int num_threads = 2;
    if (argc >= 2)
        num_threads = strtol(argv[1], NULL, 10);
    if (num_threads <= 0 || num_threads > 100)
    {
        SHELL_STDERR("Invalid number of threads: %i\n", num_threads);
        SHELL_STDOUT("Syntax: %s $num_threads\n", argv[0]);
        return;
    }
    int* thread_ids = malloc(sizeof(int) * num_threads);
    struct thread** threads = malloc(sizeof(struct thread*) * num_threads);
    SHELL_STDOUT("Starting %d threads\n", num_threads);
    for (int i = 0; i < num_threads; ++i)
    {
        thread_ids[i] = i;
        threads[i] = thread_create(thread_demo, &thread_ids[i]);
    }
    SHELL_STDOUT("Joining...\n", num_threads);
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
        SHELL_STDOUT("Syntax: %s {on,off}\n", argv[0]);
        return;
    }
    errval_t err = aos_rpc_set_led(get_init_rpc(), switch_on);
    if (err_is_ok(err))
    {
        SHELL_STDOUT("LED %sabled\n", switch_on ? "en" : "dis");
        return;
    }
    DEBUG_ERR(err, "Error switching led status");
}

static void handle_memtest(char* const argv[], int argc)
{
    SHELL_STDERR("** DISCLAIMER **\n");
    SHELL_STDERR("strtol doesnt support values above 1<<31!\n");
    if (argc != 3)
    {
        printf("Syntax: %s base_address size\n", argv[0]);
        return;
    }
    lpaddr_t base = strtol(argv[1], NULL, 0);
    size_t size = strtol(argv[2], NULL, 0);
    SHELL_STDOUT("Testing memory from 0x%08x to 0x%08x [size = 0x%08x]...\n",
        (unsigned int)base, (unsigned int)(base + size), (unsigned int)size);
    errval_t err = aos_rpc_memtest(get_init_rpc(), base, size);
    if (err_is_ok(err))
    {
        printf("Test finished\n");
        return;
    }
    DEBUG_ERR(err, "Error in memory test");
}

static void handle_oncore(char* const argv[], int argc)
{
    if (argc < 3)
    {
        SHELL_STDOUT("Syntax: %s core_id binary_name...\n", argv[0]);
        return;
    }
    coreid_t core_id = strtol(argv[1], NULL, 0);
    domainid_t new_pid;
    errval_t err = aos_rpc_process_spawn_with_args(get_init_rpc(), core_id,
        &argv[2], argc-2, &new_pid);
    if (err_is_ok(err))
    {
        SHELL_STDOUT("Test finished\n");
        return;
    }
    DEBUG_ERR(err, "Error in memory test");
}

/**
Filesystem commands:
    - [OK] pwd
    - [OK] cd
    - [OK] ls
    - [OK] cat
    - grep
*/
static void handle_pwd(char* const argv[], int argc)
{
    SHELL_STDOUT("%s\n", shell_get_state()->wd);
}

static void handle_cd(char* const argv[], int argc)
{
    assert(argc > 0);
    char* mod = argc == 1 ? "" : argv[1];
    char* new_path = shell_read_absolute_path(shell_get_state(), mod);
    // Valid path?
    fs_dirhandle_t handle;
    if (err_is_ok(opendir(new_path, &handle)))
    {
        free(shell_get_state()->wd);
        shell_get_state()->wd = new_path;
        closedir(handle);
    }
    else
    {
        SHELL_STDERR("Invalid directory: %s\n", new_path);
        free(new_path);
    }
}

static void do_ls(const char* path)
{
    fs_dirhandle_t handle;
    errval_t err = opendir(path, &handle);
    if (err_is_fail(err))
    {
        SHELL_STDERR("Unable to open directory '%s'\n", path);
        return;
    }
    char* name;
    while (err_is_ok(readdir(handle, &name)))
    {
        SHELL_STDOUT("%s\t", name);
    }
    SHELL_STDOUT("\n");
    closedir(handle);
}

static void handle_ls(char* const argv[], int argc)
{
    if (argc == 1)
        do_ls(shell_get_state()->wd);
    else
        for (int i = 1; i < argc; ++i)
        {
            if (argc > 2)
                printf("\t%s:\n", argv[i]);
            char* new_path = shell_read_absolute_path(shell_get_state(), argv[i]);
            do_ls(new_path);
            free(new_path);
        }
}


static void do_cat(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f)
    {
        SHELL_STDERR("Unable to open file '%s'\n", path);
        return;
    }
    int c;
    while ((c = fgetc(f)) != EOF)
        SHELL_STDOUT("%c", c);
    fclose(f);
}

static void handle_cat(char* const argv[], int argc)
{
    if (argc == 1)
        printf("Syntax: %s file1 file2...\n", argv[0]);
    else
        for (int i = 1; i < argc; ++i)
        {
            char* new_path = shell_read_absolute_path(shell_get_state(), argv[i]);
            do_cat(new_path);
            free(new_path);
        }
}

/**
Commands handling
*/
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
        {.name = "cat",         .handler = handle_cat},
        {.name = "cd",          .handler = handle_cd},
        {.name = "echo",        .handler = handle_echo},
        {.name = "help",        .handler = handle_help},
        {.name = "led",         .handler = handle_led},
        {.name = "ls",          .handler = handle_ls},
        {.name = "memtest",     .handler = handle_memtest},
        {.name = "oncore",      .handler = handle_oncore},
        {.name = "ps",          .handler = handle_ps},
        {.name = "pwd",         .handler = handle_pwd},
        {.name = "threads",     .handler = handle_threads},
        {.name = NULL}
    };
    return commandsTable;
}
