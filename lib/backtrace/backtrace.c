#include <aos/aos.h>
#include <backtrace.h>

extern int get_fp(void);
void backtrace(void)
{
    int topfp = get_fp();
    backtrace_from_fp(topfp);
}

void backtrace_from_fp(int topfp)
{
    const char* args = "addr2line -f -e armv7/sbin/init";
    debug_printf("Backtrace from fp @ 0x%08x\n", topfp);
    for (int i=0; i < 10 && topfp; ++i) {
        int fp = *(((int*)topfp) -3);
        //int sp = *(((int*)topfp) -2);
        int lr = *(((int*)topfp) -1);
        int pc = *(((int*)topfp) -0);
        debug_printf("i=%d fp=%.p lr=%.p pc=%.p\n", i, fp, lr, pc);

        if (i == 0)
        {
            debug_printf("%s 0x%08x\n", args, pc);
            if (!pc)
                break;
        }
        if (fp != 0)
        {
            debug_printf("%s 0x%08x\n", args, lr);
            if (!lr)
                break;
        }
        else
        {
            debug_printf("%s 0x%08x\n", args, pc);
            if (!pc)
                break;
        }
        topfp = fp;
    }
}
