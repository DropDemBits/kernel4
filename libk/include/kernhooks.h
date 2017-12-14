#ifndef __KERNHOOKS_H__
#define __KERNHOOKS_H__

#if __STDC_HOSTED__ == 0
extern void kabort();
extern void kputchar(int ic);
extern void kexit(int status);
extern void kflush();
#endif

#endif /* __KERNHOOKS_H__ */
