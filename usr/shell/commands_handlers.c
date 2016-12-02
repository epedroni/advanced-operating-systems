#include "shell.h"


bool shell_execute_command(char* const argv[], int argc)
{
    debug_printf("Execute command '%s' with %d arguments\n", argv[0], argc);
    for (int i = 1; i < argc; ++i)
        debug_printf("arg[%d]: '%s'\n", i, argv[i]);
    return true;
}
