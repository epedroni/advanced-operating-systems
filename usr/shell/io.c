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

/*
    \X... -> X...
*/
static void unescape_chars(size_t begin, size_t end, char* string)
{
    for (; begin < end-1; ++begin)
    {
        if (string[begin] == '\\')
        {
            if (string[begin+1] == 'n')
                string[begin+1] = '\n';
            if (string[begin+1] == 't')
                string[begin+1] = '\t';
            for (int i = begin; i < end - 1; ++i)
                string[i] = string[i+1];
            string[end - 1] = 0;
            --end;
        }
    }
}

static void advance_to_end_of_arg(size_t* begin, size_t* i, char* line, size_t end_pos)
{
    // Quoted args
    if (line[*i] == '"')
    {
        size_t quote_end = *i + 1;
        int escape_count = 0;
        while (quote_end < end_pos)
        {
            if (line[quote_end] == '"' && (escape_count % 2) == 0)
                break;
            if (line[quote_end] == '\\')
                ++escape_count;
            else
                escape_count = 0;
            ++quote_end;
        }
        if (quote_end != end_pos) // We found end of quote
        {
            assert(line[quote_end] == '"');
            unescape_chars(*i, quote_end, line);
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
    shell_putchar('\b');
    shell_putchar(' ');
    shell_putchar('\b');
}

// Does not include '\n' or '\r'
errval_t shell_read_line(char** to, size_t* end_pos)
{
    bool inside_quote = false;
    int escape_count = 0;
    size_t pos = 0;
    size_t buf_size = 16; // Init buffer size
    bool* inside_quote_buf = malloc(buf_size);
    *to = malloc(buf_size);

    while (true)
    {
        char ret_char;
        shell_getchar(&ret_char);
        if (ret_char == 0)
            continue;
        if (ret_char == '\r')
            ret_char = '\n';

        if (ret_char == 127)
        {
            if (pos && (*to)[pos-1] != '\n')
            {
                --pos;
                do_backslash();
                --escape_count;
                if (escape_count < 0)
                    escape_count = 0;
                if (pos > 0)
                    inside_quote = inside_quote_buf[pos-1];
                else
                    inside_quote = false;
            }
            else
                shell_putchar('\a');
            continue;
        }
        if (pos == buf_size)
        {
            buf_size *= 2;
            *to = realloc(*to, buf_size);
            inside_quote_buf = realloc(inside_quote_buf, buf_size);
        }
        if (ret_char == '"')
        {
            if (inside_quote)
            {
                if ((escape_count % 2) == 0)
                    inside_quote = false;
            }
            else
                inside_quote = true;
        }
        if (!inside_quote && ret_char == '\n')
        {
            (*to)[pos] = 0;
            *end_pos = pos;
            shell_putchar('\n');
            free(inside_quote_buf);
            return SYS_ERR_OK;
        }
        if (ret_char == '\\')
            ++escape_count;
        else
            escape_count = 0;

        (*to)[pos] = ret_char;
        inside_quote_buf[pos] = inside_quote;
        ++pos;
        shell_putchar(ret_char);
        if (ret_char == '\n' && inside_quote)
        {
            shell_putchar(' ');
            shell_putchar('>');
        }
    }
}
