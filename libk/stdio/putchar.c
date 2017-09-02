#include <stdio.h>
#if __STDC_HOSTED__ == 0
extern void kputchar(int ic);
#endif

int putchar(int ic)
{
#if __STDC_HOSTED__ == 1
    //TODO: Make write to STDOUT
#else
    kputchar(ic);
#endif
    return ic;
}
