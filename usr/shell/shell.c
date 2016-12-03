/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */

#include "shell.h"
#include <stdio.h>
#include <aos/aos.h>

static struct shell_state state;
struct shell_state* shell_get_state(void)
{
    return &state;
}

static errval_t shell_configure_output(char*** argv, int* argc)
{
    state.err = stderr;
    state.out = stdout;
    // Detect "... > file"
    if (*argc > 2)
    {
        bool write_to = !strcmp((*argv)[*argc - 2], ">");
        bool write_append = !strcmp((*argv)[*argc - 2], ">>");
        if (write_to || write_append)
        {
            char* file_path = shell_read_absolute_path(state.wd, state.home, (*argv)[*argc - 1]);
            FILE* f = fopen(file_path, write_append ? "a+" : "w+");
            free(file_path);
            if (!f)
            {
                printf("Can't open file '%s' for writing.\n", file_path);
                return SHELL_ERR_BAD_REDIRECT;
            }
            *argc -= 2;
            state.out = f;
        }
    }
    return SYS_ERR_OK;
}

errval_t shell_run(void){
    // Init
    shell_get_state()->home = "/home/pandabeer";
    shell_get_state()->wd = strdup(shell_get_state()->home);

    char** argv;
	char* line;
    int argc;
    while (true)
    {
        SHELL_PRINTF("sh:%s>", state.wd);
        errval_t err = shell_read_command(&argv, &line, &argc);
        if (err == SHELL_ERR_READ_CMD_TRY_AGAIN)
            continue;
        if (err_is_fail(err))
            return err;
        if (err_is_ok(shell_configure_output(&argv, &argc)))
            if (!shell_execute_command(argv, argc))
                SHELL_PRINTF("No such command: '%s'\n", argv[0]);
        if (state.err != stderr)
            fclose(state.err);
        if (state.out != stdout)
            fclose(state.out);
        free(argv);
		free(line);
    }
    return SYS_ERR_OK;
}


int main(int argc, char *argv[])
{
    errval_t err = shell_setup_file_system();
    if (err_is_fail(err))
    {
        DEBUG_ERR(err, "Unable to start filesystem. Abording.");
        return 0;
    }
    shell_run();
    return 0;
}
