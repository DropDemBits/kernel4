#include <common/types.h>

#ifndef __ARCH_X86_ELF_H__
#define __ARCH_X86_ELF_H__ 1

#define elf_arch_checks(x) \
    ((x).e_machine == ELF_ARCH && \
     (x).e_ident[EI_DATA] == ELF_DATA && \
     (x).e_ident[EI_CLASS] == ELF_CLASS)

#ifdef __i386__

#define ELF_CLASS ELFCLASS32
#define ELF_DATA  ELFDATA2LSB
#define ELF_ARCH  EM_X86

#else

#define ELF_CLASS ELFCLASS64
#define ELF_DATA  ELFDATA2LSB
#define ELF_ARCH  EM_X86_64

#endif

#endif /* __ARCH_ELF_H__ */