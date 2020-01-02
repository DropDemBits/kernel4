#include <string.h>

#include <common/elf.h>
#include <common/mm/liballoc.h>
#include <common/errno.h>

int elf_parse(struct inode* file, struct elf_data** data)
{
    if(file == NULL || data == NULL)
        return -EINVAL;
    
    // TODO: Check if the file has been opened yet

    struct elf_header header;
    struct elf_data* elf_data = kmalloc(sizeof(struct elf_data));

    vfs_read(file, 0, sizeof(header), &header);

    // Basic checks
    if(!CHECK_MAGIC(header.e_ident))
        return -EINVAL; // Invalid magic
    if(header.e_ident[EI_VERSION] != EV_CURRENT)
        return -EINVAL; // Too new, too old, or no version

    // Do architechture-specific checks (i.e. e_machine, e_ident[EI_DATA], e_ident[EI_CLASS])
    if(!elf_arch_checks(header))
        return -EINVAL;

    // More checks need to be done here

    // Fill up the elf_data bits
    elf_data->version = header.e_ident[EI_VERSION];
    elf_data->type = header.e_type;
    elf_data->phnum = header.e_phnum;
    elf_data->entry_point = header.e_entry;
    elf_data->flags = header.e_flags;
    elf_data->file = file;

    size_t phndx = 0;
    size_t phoff = header.e_phoff;

    elf_data->phdrs = kmalloc(header.e_phentsize * header.e_phnum);

    // Parse the elf program headers
    while(phndx < header.e_phnum)
    {
        // TODO: Validate that the target addresses don't touch kernel space
        vfs_read(file, phoff, header.e_phentsize, &elf_data->phdrs[phndx++]);
        phoff += header.e_phentsize;
    }

    *data = elf_data;
    return 0;
}

int elf_put(struct elf_data* data)
{
    if(data == NULL)
        return -EINVAL;

    kfree(data->phdrs);
    vfs_close(data->file);
    kfree(data);
    return 0;
}
