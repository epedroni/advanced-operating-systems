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
    for (int i=0; i < 10 && topfp; i++) {
        int fp = *(((int*)topfp) -3);
        //int sp = *(((int*)topfp) -2);
        int lr = *(((int*)topfp) -1);
        int pc = *(((int*)topfp) -0);
        if ( i==0 )
            printf("%s 0x%08x\n", args, pc);
        if (fp != 0)
            printf("%s 0x%08x\n", args, lr);
        else
            printf("%s 0x%08x\n", args, pc);
        topfp = fp;
    }
}
