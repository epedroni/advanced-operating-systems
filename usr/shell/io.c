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

static bool advance_to_begin_of_arg(size_t* i, char* line, size_t end_pos)
{
    while (shell_isspace(line[*i]) && *i < end_pos)
        ++(*i);
    return *i < end_pos;
}
static void advance_to_end_of_arg(size_t* begin, size_t* i, char* line, size_t end_pos)
{
    // Quoted args
    if (line[*i] == '"')
    {
        size_t quote_end = *i + 1;
        while (line[quote_end] != '"' && quote_end < end_pos)
            ++quote_end;
        if (quote_end != end_pos) // We found end of quote
        {
            assert(line[quote_end] == '"');
            *i = quote_end;
            ++(*begin);
            return;
        }
    }
    // Just go until next space
    while (!shell_isspace(line[*i]) && *i < end_pos)
        ++(*i);
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
    *out_line = line;
    *argc = 0;
    size_t argv_size = 4;
    *argv = malloc(sizeof(char*) * argv_size);
    while (begin_pos != end_pos)
    {
        if (!advance_to_begin_of_arg(&begin_pos, line, end_pos))
            return SYS_ERR_OK;
        size_t arg_end = begin_pos;
        advance_to_end_of_arg(&begin_pos, &arg_end, line, end_pos);
        if (*argc == argv_size)
        {
            argv_size *= 2;
            *argv = realloc(*argv, sizeof(char*) * argv_size);
        }
        (*argv)[*argc] = &line[begin_pos];
        line[arg_end] = 0;
        ++(*argc);
        begin_pos = arg_end+1;
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
