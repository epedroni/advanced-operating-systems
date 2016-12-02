#ifndef _HEADER_SHELL
#define _HEADER_SHELL

#include <aos/aos.h>

errval_t shell_run(void);
errval_t shell_read_line(char** to, size_t* end_pos);
errval_t shell_read_command(char*** argv, char** line, int* argc);
bool shell_execute_command(char* const argv[], int argc);

typedef void (*command_handler_fn)(char* const argv[], int argc);
struct command_handler_entry
{
    const char* name;
    command_handler_fn handler;
};

struct command_handler_entry* shell_get_command_table(void);

#define SHELL_PRINTF(...) debug_printf(__VA_ARGS__)

#endif
