#include "shell.h"

/**
Commands:
    - [OK] echo
    - led
    - threads
    - memtest
    - oncore
    - ps
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
        {.name = "echo",        .handler = handle_echo},
        {.name = "help",        .handler = handle_help},
        {.name = "args",        .handler = handle_args},
        {.name = NULL}
    };
    return commandsTable;
}
