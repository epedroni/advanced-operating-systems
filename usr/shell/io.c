#include "shell.h"
#include <aos/aos_rpc.h>
#include <ctype.h>

static bool shell_isspace(char c)
{
    switch (c)
    {
        case ' ':
        case '\t':
            return true;
    }
    return false;
}
errval_t shell_read_command(char*** argv, char** out_line, int* argc)
{
    char* line;
    size_t end_pos;
    ERROR_RET1(shell_read_line(&line, &end_pos));

    // Skip initial spaces
    size_t begin_pos = 0;
    while (shell_isspace(line[begin_pos]) && begin_pos < end_pos)
        ++begin_pos;
    // Skip final white spaces
    if (begin_pos != end_pos)
    {
        --end_pos;
        while (shell_isspace(line[end_pos]) && begin_pos < end_pos)
            --end_pos;
        ++end_pos;
        line[end_pos] = 0;
    }
    if (begin_pos == end_pos)
    {
        free(line);
        return SHELL_ERR_READ_CMD_TRY_AGAIN;
    }

    // And now fill all arguments
    *argc = 1;
    size_t argv_size = 4;
    *argv = malloc(sizeof(char*) * argv_size);
    (*argv)[0] = &line[begin_pos];
    for (int i = begin_pos; i < end_pos; ++i)
    {
        // TODO: Handle '"' to group arguments
        if (shell_isspace(line[i]))
        {
            line[i] = 0;
            do
            {
                ++i;
                assert(i < end_pos); // Because we skipped spaces in the end!
            }
            while (shell_isspace(line[i]));
            if (*argc == argv_size)
            {
                argv_size *= 2;
                *argv = realloc(*argv, sizeof(char*) * argv_size);
            }
            (*argv)[*argc] = &line[i];
            ++(*argc);
        }
    }

    return SYS_ERR_OK;
}

static void do_backslash(void)
{
    aos_rpc_serial_putchar(get_init_rpc(), '\b');
    aos_rpc_serial_putchar(get_init_rpc(), ' ');
    aos_rpc_serial_putchar(get_init_rpc(), '\b');
}

// Does not include '\n' or '\r'
errval_t shell_read_line(char** to, size_t* end_pos)
{
    size_t pos = 0;
    size_t buf_size = 16; // Init buffer size
    *to = malloc(buf_size);

    while (true)
    {
		char ret_char;
		aos_rpc_serial_getchar(get_init_rpc(), &ret_char);
        if (ret_char == 0)
            continue;
		if (ret_char == 127)
        {
            if (pos)
            {
                --pos;
                do_backslash();
            }
            else
                aos_rpc_serial_putchar(get_init_rpc(), '\a');
            continue;
        }
        if (pos == buf_size)
        {
            buf_size *= 2;
            *to = realloc(*to, buf_size);
        }
        if (ret_char == '\r' || ret_char == '\n')
        {
            (*to)[pos] = 0;
            *end_pos = pos;
    		aos_rpc_serial_putchar(get_init_rpc(), '\n');
            return SYS_ERR_OK;
        }
        (*to)[pos] = ret_char;
        ++pos;
		aos_rpc_serial_putchar(get_init_rpc(), ret_char);
	}
}
