#include <string.h>

#include <common/elf.h>

#define EINVAL 1

int elf_parse(vfs_inode_t* file, struct elf_data** data)
{
    if(file == NULL || data == NULL)
        return -EINVAL;
    
    // TODO: Check if the file has been opened yet

    union elf_header header;
    struct elf_data* elf_data = kmalloc(sizeof(struct elf_data));

    size_t phoff = 0;
    size_t phentsize = 0;
    size_t phnum = 0;
    size_t phend = 0;

    vfs_read(file, 0, sizeof(header), &header);

    // Check the ELF Magic
    if(!CHECK_MAGIC(header.elf.e_ident))
        return -EINVAL; // Invalid magic

    // Check the version
    if(header.elf.e_ident[EI_VERSION] != EV_CURRENT)
        return -EINVAL; // Too new, too old, or no version

    // TODO: Do arch verification

    // Fill up the elf_data bits
    elf_data->version = header.elf.e_ident[EI_VERSION];
    elf_data->type = header.elf.e_type;
    elf_data->file = file;
    elf_data->prog_heads = NULL;
    elf_data->prog_tail = NULL;

    // TODO: Do arch fillup

    if(header.elf.e_ident[EI_CLASS] == ELFCLASS32)
    {
        elf_data->entry_point = header.elf32.e_entry;
        elf_data->flags = header.elf32.e_flags;

        phoff = header.elf32.e_phoff;
        phentsize = header.elf32.e_phentsize;
        phnum = header.elf32.e_phnum;
    }
    else
    {
        elf_data->entry_point = header.elf64.e_entry;
        elf_data->flags = header.elf64.e_flags;

        phoff = header.elf64.e_phoff;
        phentsize = header.elf64.e_phentsize;
        phnum = header.elf64.e_phnum;
    }

    // Parse the elf program headers
    phend = phentsize * phnum + phoff;

    while(phoff < phend)
    {
        union elf_common_proghead file_proghead;
        vfs_read(file, phoff, phentsize, &file_proghead);

        if(file_proghead.elf32.p_type == PT_NULL)
            goto skip_proghead; // Skip NULL segments
        
        struct elf_proghead* proghead_data = kmalloc(sizeof(struct elf_proghead));
        proghead_data->next = NULL;

        // Fill up the proghead representation
        if(header.elf.e_ident[EI_CLASS] == ELFCLASS32)
        {
            proghead_data->type = file_proghead.elf32.p_type;
            proghead_data->flags = file_proghead.elf32.p_flags;
            proghead_data->file_offset = file_proghead.elf32.p_offset;
            proghead_data->file_size = file_proghead.elf32.p_filesz;
            proghead_data->vaddr = file_proghead.elf32.p_vaddr;
            proghead_data->vsize = file_proghead.elf32.p_memsz;
            proghead_data->align = file_proghead.elf32.p_align;
        }
        else
        {
            proghead_data->type = file_proghead.elf64.p_type;
            proghead_data->flags = file_proghead.elf64.p_flags;
            proghead_data->file_offset = file_proghead.elf64.p_offset;
            proghead_data->file_size = file_proghead.elf64.p_filesz;
            proghead_data->vaddr = file_proghead.elf64.p_vaddr;
            proghead_data->vsize = file_proghead.elf64.p_memsz;
            proghead_data->align = file_proghead.elf64.p_align;
        }

        // Append on to the end of the list
        if(elf_data->prog_heads == NULL)
        {
            elf_data->prog_heads = proghead_data;
            elf_data->prog_tail = proghead_data;
        }
        else
        {
            elf_data->prog_tail->next = proghead_data;
        }

        skip_proghead:
        phoff += phentsize;
    }

    *data = elf_data;
    return 0;
}

int elf_put(struct elf_data* data)
{
    if(data == NULL)
        return -EINVAL;

    struct elf_proghead* proghead = data->prog_heads;
    while(proghead != NULL)
    {
        struct elf_proghead* temp = proghead;
        proghead = temp->next;

        kfree(proghead);
    }

    vfs_close(data->file);
    kfree(data);
    return 0;
}
