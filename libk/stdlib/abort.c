#include <stdlib.h>
#if __STDC_HOSTED__ == 0
void kpanic(const char* message);
#endif

__attribute__((__noreturn__))
void abort(void)
{
#if __STDC_HOSTED__ == 1
    //TODO: Raise abort signal
    printf("abort()\n");
    while (1);
#else
    //Panic
    kpanic("Abnormal exit");
#endif
    __builtin_unreachable();
}
