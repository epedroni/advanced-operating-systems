#include <aos/aos_rpc.h>
#include <fs/fs.h>
#include <fs/dirent.h>

#include "shell.h"

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

static int thread_memtest(void* tid)
{
    int size = (int)tid;
    SHELL_STDOUT("Testing memory from 0x%08x...\n",
        (unsigned int)size);
    void* buf = malloc(size);
    memset(buf, 0, size);
    free(buf);
    return 0;
}

static void handle_memtest(char* const argv[], int argc)
{
    if (argc != 2)
    {
        printf("Syntax: %s size\n", argv[0]);
        return;
    }
    int base = strtol(argv[1], NULL, 0);
    int retval;
    struct thread* test_thread = thread_create(thread_memtest, (void*)base);
    thread_join(test_thread, &retval);
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
    - [OK] grep
*/
static void handle_pwd(char* const argv[], int argc)
{
    SHELL_STDOUT("%s\n", shell_get_state()->wd);
}

static void handle_cd(char* const argv[], int argc)
{
    assert(argc > 0);
    char* mod = argc == 1 ? "" : argv[1];
    char* new_path = shell_read_absolute_path(shell_get_state()->wd, shell_get_state()->home, mod);
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

static void do_ls(void* data, const char* rel_path, const char* abs_path)
{
    fs_dirhandle_t handle;
    errval_t err = opendir(abs_path, &handle);
    if (err_is_fail(err))
    {
        SHELL_STDERR("Unable to open directory '%s'\n", rel_path);
        return;
    }
    char* name;
    while (err_is_ok(readdir(handle, &name)))
    {
        SHELL_STDOUT("%s\t", name);
        free(name);
    }
    SHELL_STDOUT("\n");
    closedir(handle);
}

static void handle_ls(char* const argv[], int argc)
{
    if (argc == 1)
        do_ls(NULL, ".", shell_get_state()->wd);
    else
        shell_fs_match_files(&argv[1], argc-1, do_ls, NULL, MATCH_FLAG_DIRECTORIES);
}


static void do_cat(void* data, const char* rel_path, const char* abs_path)
{
    FILE* f = fopen(abs_path, "r");
    if (!f)
    {
        SHELL_STDERR("Unable to open file '%s'\n", rel_path);
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
        SHELL_STDOUT("Syntax: %s file1 file2...\n", argv[0]);
    else
        shell_fs_match_files(&argv[1], argc-1, do_cat, NULL, MATCH_FLAG_FILES);
}

static void do_wc(void* data, const char* rel_path, const char* abs_path)
{

    FILE* f = fopen(abs_path, "r");
    if (!f)
    {
        SHELL_STDERR("Unable to open file '%s'\n", rel_path);
        return;
    }
    int c;
    int words = 0;
    int lines = 0;
    int characters = 0;
    bool in_word = false;
    while ((c = fgetc(f)) != EOF)
    {
        ++characters;
        if (c == '\n')
            ++lines;
        // New word?
        bool space = c == '\n' || c == '\t' || c == ' ';
        if (!in_word && !space)
        {
            ++words;
            in_word = true;
        }
        else if (in_word && space)
            in_word = false;
    }
    fclose(f);
    SHELL_STDOUT("%d %d %d\n", lines, words, characters);
}

static void handle_wc(char* const argv[], int argc)
{
    if (argc <= 1)
    {
        SHELL_STDOUT("Syntax: %s file1 [file2 [...]]\n", argv[0]);
        return;
    }

    shell_fs_match_files(&argv[1], argc-1, do_wc, NULL, MATCH_FLAG_FILES);
}

static char* readline(FILE* f)
{
    size_t buf_size = 16;
    size_t buf_pos = 0;
    char* buf = malloc(buf_size);
    int c;
    while ((c = fgetc(f)))
    {
        if (c == EOF)
        {
            if (buf_pos == 0)
            {
                free(buf);
                return NULL;
            }
            break;
        }
        if (c == '\n')
            break;
        if (buf_pos + 2 >= buf_size)
        {
            buf_size *= 2;
            buf = realloc(buf, buf_size);
        }
        buf[buf_pos] = c;
        ++buf_pos;
    }
    buf[buf_pos] = 0;
    return buf;
}

static void do_grep(void* data, const char* rel_path, const char* abs_path)
{
    FILE* f = fopen(abs_path, "r");
    if (!f)
    {
        SHELL_STDERR("Unable to open file '%s'\n", rel_path);
        return;
    }
    char* line;
    int line_num = 1;
    while ((line = readline(f)))
    {
        if (strstr(line, (const char*)data))
            SHELL_STDOUT("%s[%d]:%s\n", rel_path, line_num, line);
        free(line);
        ++line_num;
    }
    fclose(f);
}

static void syntax_grep(void)
{
    SHELL_STDOUT("Syntax: grep [-r] pattern file1 file2...\n");
}

static void handle_grep(char* const argv[], int argc)
{
    // Handle args
    int flags = MATCH_FLAG_FILES;
    int pattern_idx = 1;
    if (argc < 3)
        return syntax_grep();
    if (!strcmp(argv[1], "-r"))
    {
        flags |= MATCH_FLAG_RECURSIVE;
        ++pattern_idx;
        if (argc < 4)
            return syntax_grep();
    }
    // Find files
    shell_fs_match_files(&argv[pattern_idx+1], argc-pattern_idx-1,
        do_grep, argv[pattern_idx], flags);
}

static void handle_fallback(char* const argv[], int argc)
{
    if (!argc)
        return;

    coreid_t core_id = 0;
    domainid_t new_pid;
    aos_rpc_process_spawn_with_args(get_init_rpc(), core_id,
        argv, argc, &new_pid);
}

/**
Commands handling
*/
bool shell_execute_command(char* const argv[], int argc)
{
    struct command_handler_entry* commands = shell_get_command_table();
    int i;
    for (i = 0; commands[i].name != NULL; ++i)
        if (!strcmp(argv[0], commands[i].name))
        {
            commands[i].handler(argv, argc);
            return true;
        }
    assert(commands[i].name == NULL);
    commands[i].handler(argv, argc);
    return false;
}

struct command_handler_entry* shell_get_command_table(void)
{
    static struct command_handler_entry commandsTable[] = {
        {.name = "args",        .handler = handle_args},
        {.name = "cat",         .handler = handle_cat},
        {.name = "cd",          .handler = handle_cd},
        {.name = "echo",        .handler = handle_echo},
        {.name = "grep",        .handler = handle_grep},
        {.name = "help",        .handler = handle_help},
        {.name = "led",         .handler = handle_led},
        {.name = "ls",          .handler = handle_ls},
        {.name = "memtest",     .handler = handle_memtest},
        {.name = "oncore",      .handler = handle_oncore},
        {.name = "ps",          .handler = handle_ps},
        {.name = "pwd",         .handler = handle_pwd},
        {.name = "threads",     .handler = handle_threads},
        {.name = "wc",          .handler = handle_wc},
        {.name = NULL,          .handler = handle_fallback}
    };
    return commandsTable;
}
