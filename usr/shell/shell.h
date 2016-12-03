#ifndef _HEADER_SHELL
#define _HEADER_SHELL

#include <aos/aos.h>

// Shell state
struct shell_state
{
    char* wd;               // Working directory
    const char* home;       // Default directory
    FILE* out;
    FILE* err;
};

// Generic shell commands
errval_t shell_run(void);
errval_t shell_read_line(char** to, size_t* end_pos);
errval_t shell_read_command(char*** argv, char** line, int* argc);
bool shell_execute_command(char* const argv[], int argc);
void shell_reset_output(void);
struct shell_state* shell_get_state(void);

// Filesystem
errval_t shell_setup_file_system(void);
char* shell_read_absolute_path(struct shell_state* state, char* path_mod);

// Shell commands handlers
typedef void (*command_handler_fn)(char* const argv[], int argc);
struct command_handler_entry
{
    const char* name;
    command_handler_fn handler;
};
struct command_handler_entry* shell_get_command_table(void);

// Utils
#define SHELL_PRINTF(...) debug_printf(__VA_ARGS__)

#endif
