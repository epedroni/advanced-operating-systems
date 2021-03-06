#include <aos/aos.h>
#include <aos/dispatch.h>
#include <backtrace.h>

extern int get_fp(void);
void backtrace(void)
{
    int topfp = get_fp();
    backtrace_from_fp(topfp);
}

void backtrace_from_fp(int topfp)
{
    const char* args = "addr2line -f -e ";
    const char* exe_name = disp_name();
    // Skip initial '/'
    exe_name = &exe_name[1];
    debug_printf("Backtrace from fp @ 0x%08x\n", topfp);
    for (int i=0; i < 10 && topfp; ++i) {
        int fp = *(((int*)topfp) -3);
        //int sp = *(((int*)topfp) -2);
        int lr = *(((int*)topfp) -1);
        int pc = *(((int*)topfp) -0);

        if (i == 0)
        {
            printf("%s %s 0x%08x\n", args, exe_name, pc);
            if (!pc)
                break;
        }
        if (fp != 0)
        {
            printf("%s %s 0x%08x\n", args, exe_name, lr);
            if (!lr)
                break;
        }
        else
        {
            printf("%s %s 0x%08x\n", args, exe_name, pc);
            if (!pc)
                break;
        }
        topfp = fp;
    }
}
