#include <stdio.h>
#include <aos/aos.h>

int main(int argc, char *argv[])
{
    debug_printf("args: received %d arguments\n", argc);
    for (int i = 0; i < argc; ++i)
        debug_printf("argv[%d] = '%s'\n", i, argv[i]);
	return 0;
}
