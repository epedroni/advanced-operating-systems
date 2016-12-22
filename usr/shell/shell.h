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
struct shell_state* shell_get_state(void);
#define SHELL_STDOUT(...) fprintf(shell_get_state()->out, __VA_ARGS__)
#define SHELL_STDERR(...) fprintf(shell_get_state()->err, __VA_ARGS__)

// Generic shell commands
errval_t shell_run(void);
errval_t shell_read_line(char** to, size_t* end_pos);
errval_t shell_read_command(char*** argv, char** line, int* argc);
bool shell_execute_command(char* const argv[], int argc);
void shell_reset_output(void);

// Filesystem
errval_t shell_setup_file_system(void);
char* shell_read_absolute_path(const char* wd, const char* home, const char* path_mod);
typedef void (*fs_callback_fn)(void* data, const char* rel_path, const char* path);

enum shell_match_files_flags
{
    MATCH_FLAGS_NONE            = 0x0,
    MATCH_FLAG_RECURSIVE        = 0x1,
    MATCH_FLAG_FILES            = 0x2,
    MATCH_FLAG_DIRECTORIES      = 0x4,
};

void shell_fs_match_files(char* const argv[], int argc, fs_callback_fn callback,
    void* data, int flags);

// Shell commands handlers
typedef void (*command_handler_fn)(char* const argv[], int argc);
struct command_handler_entry
{
    const char* name;
    command_handler_fn handler;
};
struct command_handler_entry* shell_get_command_table(void);

// IO Driver
errval_t shell_setup_io_driver(void);
void shell_putchar(char c);
void shell_getchar(char* c);
bool shell_isspace(char c);

// Utils
#define SHELL_PRINTF(...) debug_printf(__VA_ARGS__)

#endif
