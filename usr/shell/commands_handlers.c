#include "shell.h"


/**
Commands:
    - [OK] echo
    - led
    - threads
    - memtest
    - oncore
    - ps
    - help
*/

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

bool shell_execute_command(char* const argv[], int argc)
{
    static struct command_handler_entry commandsTable[] = {
        {.command = "echo",     .handler = handle_echo},
        {.command = "args",     .handler = handle_args}
    };
    for (int i = 0; i < sizeof(commandsTable) / sizeof(commandsTable[0]); ++i)
        if (!strcmp(argv[0], commandsTable[i].command))
        {
            commandsTable[i].handler(argv, argc);
            return true;
        }
    return false;
}
