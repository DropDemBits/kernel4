/**
 * Copyright (C) 2018 DropDemBits
 * 
 * This file is part of Kernel4.
 * 
 * Kernel4 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kernel4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kernel4.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <string.h>
#include <stdio.h>

#include <common/fs/tarfs.h>
#include <common/mm/mm.h>

/*
typedef struct vfs_inode {
    // name in dirent
    uint32_t perms_mask : 12; // Access permissions
    uint32_t type : 4; // Node type
    uint32_t uid;    // User ID
    uint32_t gid;    // Group ID
    uint32_t size;    // Size in bytes
    ino_t inode; // Associated fs inode
    vfs_read_func_t read;
    vfs_write_func_t write;
    vfs_open_func_t open;
    vfs_close_func_t close;
    vfs_readdir_func_t readdir;
    vfs_finddir_func_t finddir;
    uint32_t impl_specific; // VFS-impl specific value
    struct vfs_inode* ptr;
} vfs_inode_t;
*/

struct tar_header
{
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char modified_time[12];
    char chksum[8];
    char typeflag[1];
    char link_filename[100];
    // USTAR format specific
    char ustar_magic[6];
    char ustar_version[2];
    char uname[32];
    char gname[32];
    char dev_major[8];
    char dev_minor[8];
    char fileprefix[155];

    char padding[12];
};

static vfs_inode_t* root_node;
static vfs_inode_t node_list[64];
static struct vfs_dirent node_dirents[64];
static struct tar_header *node_data[64];
static size_t num_nodes = 0;

static unsigned int getsize(const char *in)
{

    unsigned int size = 0;
    unsigned int j;
    unsigned int count = 1;

    for (j = 11; j > 0; j--, count *= 8)
        size += ((in[j - 1] - '0') * count);

    return size;
}

static ssize_t tarfs_read(vfs_inode_t *node, size_t off, size_t len, uint8_t* buffer)
{
    uint8_t* data = (uint8_t*)(node_data[node->inode] + 1);
    size_t size = getsize(node_data[node->inode]->size);
    if(size == 0)
        return -1;
    else if(off >= size) // Offset exceeding bounds
        return 0;

    size_t read_len = 0;
    for(read_len = 0; read_len < len && read_len < size; read_len++)
    {
        buffer[read_len] = data[off+read_len];
    }

    return read_len;
}

static struct vfs_dirent* tarfs_readdir(vfs_inode_t *node, size_t index)
{
    if(index >= num_nodes)
        return KNULL;
    return &node_dirents[index];
}

static vfs_inode_t* tarfs_finddir(vfs_inode_t *node, const char* name)
{
    for(int i = 0; i < num_nodes; i++)
    {
        if(strcmp(name, node_data[i]->filename) == 0)
            return &node_list[i];
    }

    return KNULL;
}

vfs_inode_t* tarfs_init(void* address, size_t tar_len)
{
    if(address == KNULL) return KNULL;
    printf("[INTRD] initrd at %p (%lld / %lld KiB)\n", address, tar_len, tar_len >> 10);

    // Identity map tarfs into address space (Temporary)
    for(size_t pages = 0; pages < ((tar_len + 0xFFF) >> 12); pages++)
    {
        mmu_map_direct((uintptr_t)address + (pages << 12), (uintptr_t)address + (pages << 12));
    }

    struct tar_header *tar_file = (struct tar_header*) address;
    if(strncmp(tar_file->ustar_magic,"ustar",6) != 0)
        return KNULL;

    int finode = 0;
    while(tar_file->filename[0])
    {
        num_nodes++;
        unsigned int size = getsize(tar_file->size);

        node_list[finode].perms_mask = 0;
        node_list[finode].inode = finode;
        node_list[finode].size = size;
        node_list[finode].ptr = KNULL;
        node_list[finode].read = tarfs_read;
        node_list[finode].write = KNULL;
        node_list[finode].open = KNULL;
        node_list[finode].close = KNULL;
        node_list[finode].readdir = tarfs_readdir;
        node_list[finode].finddir = tarfs_finddir;
        switch(tar_file->typeflag[0])
        {
            case 0:
            case '0':
                node_list[finode].type = VFS_TYPE_FILE;
                break;
            case '3':
                node_list[finode].type = VFS_TYPE_CHARDEV;
                break;
            case '4':
                node_list[finode].type = VFS_TYPE_BLOCKDEV;
                break;
            case '5':
                node_list[finode].type = VFS_TYPE_DIRECTORY;
                break;
            case '6':
                node_list[finode].type = VFS_TYPE_PIPE;
                break;
        }
        node_data[finode] = tar_file;
        node_dirents[finode].inode = finode;
        // TODO: Strip filepath
        node_dirents[finode].name = tar_file->filename;

        finode++;
        tar_file += ((size >> 9) + 1);
        if(size % 512)
            tar_file++;
    }

    root_node = kmalloc(sizeof(vfs_inode_t));
    root_node->perms_mask = 0;
    root_node->type = VFS_TYPE_DIRECTORY;
    root_node->inode = 0;
    root_node->size = root_node->uid = root_node->gid = root_node->impl_specific = 0;
    root_node->ptr = KNULL;
    root_node->read = tarfs_read;
    root_node->write = KNULL;
    root_node->open = KNULL;
    root_node->close = KNULL;
    root_node->readdir = tarfs_readdir;
    root_node->finddir = tarfs_finddir;

    return root_node;
}
