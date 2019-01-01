#include <common/types.h>
#include <common/fs/vfs.h>

#ifndef __ELF_H__
#define __ELF_H__ 1

/** ELF Header Related **/

// Indicies
#define EI_MAG0     0   // = x7F
#define EI_MAG1     1   // = 'E'
#define EI_MAG2     2   // = 'L'
#define EI_MAG3     3   // = 'F'
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION  6
#define EI_PAD      7
#define EI_NIDENT   16

// Values
#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASS32      1
#define ELFCLASS64      2

#define ELFDATANONE     0
#define ELFDATA2LSB     1
#define ELFDATA2MSB     2

#define EV_NONE         1
#define EV_CURRENT      1

#define EI_ABI_SYSTEMV 0

#define ET_NONE     0x00
#define ET_REL      0x01
#define ET_EXEC     0x02
#define ET_DYN      0x03
#define ET_CORE     0x04
#define ET_LOOS     0xfe00
#define ET_HIOS     0xfeff
#define ET_LOPROC   0xff00
#define ET_HIPROC   0xffff

#define EM_NONE    0x00
#define EM_SPARC   0x02
#define EM_X86     0x03    // Alias to 386
#define EM_MIPS    0x08
#define EM_PPC     0x14
#define EM_S390    0x16
#define EM_ARM     0x2A
#define EM_SH      0x28
#define EM_IA64    0x32
#define EM_X86_64  0x3E
#define EM_AARCH64 0xB7
#define EM_RISCV   0xF3

/** Program Header Related **/
// Segment Type
#define PT_NULL     0x00000000
#define PT_LOAD     0x00000001
#define PT_DYNAMIC  0x00000002
#define PT_INTERP   0x00000003
#define PT_NOTE     0x00000004
#define PT_SHLIB    0x00000005
#define PT_PHDR     0x00000006
#define PT_LOOS     0x60000000
#define PT_HIOS     0x6FFFFFFF
#define PT_LOPROC   0x70000000
#define PT_HIPROC   0x7FFFFFFF

/** Section Header Related **/
// Special Indicies
#define SHN_UNDEF            0
#define SHN_LORESERVE   0xff00
#define SHN_LOPROC      0xff00
#define SHN_HIPROC      0xff1f
#define SHN_ABS         0xfff1
#define SHN_COMMON      0xfff2
#define SHN_HIRESERVE   0xffff

// Section Type
#define SHT_NULL            0x0
#define SHT_PROGBITS        0x1
#define SHT_SYMTAB          0x2
#define SHT_STRTAB          0x3
#define SHT_RELA            0x4
#define SHT_HASH            0x5
#define SHT_DYNAMIC         0x6
#define SHT_NOTE            0x7
#define SHT_NOBITS          0x8
#define SHT_REL             0x9
#define SHT_SHLIB           0xA
#define SHT_DYNSYM          0xB

#define SHT_INIT_ARRAY      0xE
#define SHT_FINI_ARRAY      0xF
#define SHT_PREINIT_ARRAY   0x10
#define SHT_GROUP           0x11
#define SHT_SYMTAB_SHNDX    0x12
#define SHT_NUM             0x13
#define SHT_LOOS            0x60000000

#define SHT_LOPROC          0x70000000
#define SHT_HIPROC          0x7fffffff
#define SHT_LOUSER          0x80000000
#define SHT_HIUSER          0xffffffff

// Section Attributes
#define SHF_WRITE           0x00000001
#define SHF_ALLOC           0x00000002
#define SHF_EXECINSTR       0x00000004
#define SHF_MERGE           0x00000010
#define SHF_STRINGS         0x00000020
#define SHF_INFO_LINK       0x00000040
#define SHF_LINK_ORDER      0x00000080
#define SHF_OS_NONCONFORM   0x00000100
#define SHF_GROUP           0x00000200
#define SHF_TLS             0x00000400
#define SHF_MASKOS          0x0ff00000
#define SHF_MASKPROC        0xf0000000
// Solaris specific
#define SHF_ORDERED         0x40000000
#define SHF_EXCLUDE         0x80000000

#define CHECK_MAGIC(file) \
    ((file)[EI_MAG0] == ELFMAG0 && \
     (file)[EI_MAG1] == ELFMAG1 && \
     (file)[EI_MAG2] == ELFMAG2 && \
     (file)[EI_MAG3] == ELFMAG3    \
    )

struct elf_header_common
{
    uint8_t e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
};

/** ELF32 Structures **/
struct elf32_header
{
    struct elf_header_common header;

    uint32_t e_entry;   // Entry Point
    uint32_t e_phoff;   // Program Header pointer
    uint32_t e_shoff;   // Section Header pointer

    uint32_t e_flags;
    uint16_t e_ehsize;

    // Program header
    uint16_t e_phentsize;
    uint16_t e_phnum;

    // Section header
    uint16_t e_shentsize;
    uint16_t e_shnum;

    // Section header string index
    uint16_t e_shstrndx;
};

struct elf32_proghead
{
    uint32_t p_type;
    
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

struct elf32_secthead
{
    uint32_t sh_name;
    uint32_t sh_type;

    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;

    uint32_t sh_link;
    uint32_t sh_info;

    uint32_t sh_addralign;
    uint32_t sh_entsize;
};

/** ELF64 Structures **/
struct elf64_header
{
    struct elf_header_common header;

    uint64_t e_entry;   // Entry Point
    uint64_t e_phoff;   // Program Header pointer
    uint64_t e_shoff;   // Section Header pointer

    uint32_t e_flags;
    uint16_t e_ehsize;

    // Program header
    uint16_t e_phentsize;
    uint16_t e_phnum;

    // Section header
    uint16_t e_shentsize;
    uint16_t e_shnum;

    // Section header string index
    uint16_t e_shstrndx;
};

struct elf64_proghead
{
    uint32_t p_type;
    uint32_t p_flags;
    
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct elf64_secthead
{
    uint32_t sh_name;
    uint32_t sh_type;

    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;

    uint32_t sh_link;
    uint32_t sh_info;

    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

/** Generic Representations **/
union elf_header
{
    struct elf_header_common elf;
    struct elf32_header elf32;
    struct elf64_header elf64;
};

union elf_common_proghead
{
    struct elf32_proghead elf32;
    struct elf64_proghead elf64;
};

struct elf_proghead
{
    struct elf_proghead* next;

    uint32_t type;
    uint32_t flags;

    // File position
    size_t file_offset;
    size_t file_size;

    // Memory location
    size_t vaddr;
    size_t vsize;

    size_t align;
};

struct elf_data
{
    uint8_t version;   // ELF Version, usually 1
    uint8_t padding;
    uint16_t type;      // File type (see ET_*)
    uint32_t flags;     // Architechture-specific flags

    size_t entry_point;
    struct elf_proghead* prog_heads;
    struct elf_proghead* prog_tail;
    vfs_inode_t* file;  // File associated with the elf data
};

/**
 * @brief Parses an elf file and returns the filled up data structure
 * The file must be a valid open file
 * The pointer given must be freed using elf_put
 * @param file The file to parse
 * @param data A pointer where the data structure will be put into
 * @return int 0 if successful, non-zero otherwise 
 */
int elf_parse(vfs_inode_t* file, struct elf_data** data);

/**
 * @brief Frees the elf data structure given
 * Automatically closes the associated file
 * The pointed data must not be used after this method
 * @param data 
 * @return int 0 if successful, non-zero otherwise
 */
int elf_put(struct elf_data* data);

#endif /* __ELF_H__ */