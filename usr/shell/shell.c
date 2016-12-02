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

errval_t shell_run(void){
    char** argv;
	char* line;
    int argc;
    while (true)
    {
        SHELL_PRINTF("shell>");
        errval_t err = shell_read_command(&argv, &line, &argc);
        if (err == SHELL_ERR_READ_CMD_TRY_AGAIN)
            continue;
        if (err_is_fail(err))
            return err;
        if (!shell_execute_command(argv, argc))
            SHELL_PRINTF("No such command: '%s'\n", argv[0]);
        free(argv);
		free(line);
    }
    return SYS_ERR_OK;
}


int main(int argc, char *argv[])
{
	shell_run();
	return 0;
}
