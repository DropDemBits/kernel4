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
#include <common/mm/liballoc.h>
#include <common/util/klog.h>

#define ROOT_INODE 1
#define NODE_LIST_BASE (ROOT_INODE + 1)
#define NUM_NODES   64
#define NUM_PREALLOC_DIRENTS 16

struct tar_header
{
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char modified_time[12];
    char chksum[8];
    char typeflag;
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

// Generic file handling
extern struct dirent* default_dnode_readdir(struct dnode *dnode, size_t index, struct dirent* dirent);
extern struct dnode* default_dnode_finddir(struct dnode *dnode, const char* path);
extern ssize_t default_write(struct inode *file_node, size_t off, size_t len, void* buffer);
extern void default_open(struct inode *file_node, int oflags);
extern void default_close(struct inode *file_node);

static struct dnode* root_dir;
static struct inode* root_node;
static struct inode* node_list[NUM_NODES];
static struct tar_header *node_data[NUM_NODES];
static size_t num_nodes = 0;

static struct inode* get_inode(struct fs_instance *instance, ino_t inode)
{
    if(inode == 0)
        return NULL;        // Bad inode
    else if(inode == ROOT_INODE)
        return root_node;   // Get the root inode
    
    return node_list[inode - NODE_LIST_BASE];
}

static unsigned int getsize(const char *in)
{
    unsigned int size = 0;
    unsigned int j;
    unsigned int count = 1;

    for (j = 11; j > 0; j--, count *= 8)
        size += ((in[j - 1] - '0') * count);

    return size;
}

static ssize_t tarfs_read(struct inode *node, size_t off, size_t len, void* buffer)
{
    uint8_t* data = (uint8_t*)(node_data[node->fs_inode - NODE_LIST_BASE] + 1);
    size_t size = getsize(node_data[node->fs_inode - NODE_LIST_BASE]->size);
    if(size == 0)
        return -1;
    else if(off >= size) // Offset exceeding bounds
        return 0;

    size_t read_len = 0;
    for(read_len = 0; read_len < len && read_len < size; read_len++)
    {
        ((uint8_t*)buffer)[read_len] = data[off+read_len];
    }

    return read_len;
}

static struct inode_ops iops = 
{
    .read = tarfs_read,
    .write = default_write,
    .open = default_open,
    .close = default_close,
};

static struct dnode_ops dops = 
{
    .read_dir = default_dnode_readdir,
    .find_dir = default_dnode_finddir,
};

static int tar_gettype(char typeflag)
{
    switch(typeflag)
    {
        case 0:
        case '0':
            return VFS_TYPE_FILE;
        case '3':
            return VFS_TYPE_CHARDEV;
        case '4':
            return VFS_TYPE_BLOCKDEV;
        case '5':
            return VFS_TYPE_DIRECTORY;
        case '6':
            return VFS_TYPE_PIPE;
    }

    return VFS_TYPE_UNKNOWN;
}

static void construct_node(struct fs_instance *instance, struct inode* node)
{
    if(node == NULL)
        return;

    memset(node, 0, sizeof(struct inode));
    node->symlink_ptr = NULL;
    node->iops = &iops;
    node->num_users_lock = mutex_create();
    node->instance = instance;
    num_nodes++;
}

static void construct_dir_node(struct dnode* dnode, struct inode* inode, const char* name, const char* path)
{
    dnode->path = path;
    dnode->name = name;
    dnode->inode = inode;
    dnode->subdirs = NULL;
    dnode->next = NULL;
    dnode->parent = NULL;

    // Replace w/ dops
    dnode->dops = &dops;
}

/*
 * All base_path's must be absolute paths (excluding the initial /)
 */
static void append_dirent(struct inode* target, char* base_path, const char* filename, const char* path)
{
    struct dnode* parent = default_dnode_finddir(root_dir, base_path);
    if(!parent)
    {
        klog_logln(LVL_ERROR, "initrd dirnode %s does not exist (yet?)", base_path);
        return;
    }

    // Setup current dirent
    struct dnode* dnode = kmalloc(sizeof(struct dnode));
    construct_dir_node(dnode, target, filename, path);

    // Pass the fs_instance down
    dnode->instance = parent->instance;

    // Append to the parent
    dnode->next = parent->subdirs;
    parent->subdirs = dnode;
}

struct fs_instance* tarfs_init(void* address, size_t tar_len)
{
    if(address == KNULL) return KNULL;

    struct tar_header *tar_file = (struct tar_header*) address;
    if(strncmp(tar_file->ustar_magic,"ustar",6) != 0)
        return KNULL;

    // Setup root node
    struct fs_instance* instance = kmalloc(sizeof(struct fs_instance));
    root_node = kmalloc(sizeof(struct inode));
    root_dir = kmalloc(sizeof(struct dnode));

    construct_node(instance, root_node);
    root_node->type = VFS_TYPE_DIRECTORY;
    root_node->fs_inode = ROOT_INODE;
    root_node->impl_specific = 0;

    // Setup root dirnode
    construct_dir_node(root_dir, root_node, "/", "/");

    // Setup fs_instance
    instance->get_inode = get_inode;
    instance->root = root_dir;

    // Setup instance
    root_dir->instance = instance;

    int finode = NODE_LIST_BASE;
    while(tar_file->filename[0])
    {
        unsigned int size = getsize(tar_file->size);
        const char* filename = strrchr(tar_file->filename, '/');
        struct inode* node = kmalloc(sizeof(struct inode));

        // Setup vfs_node
        node_list[finode - NODE_LIST_BASE] = node;
        construct_node(instance, node);
        node->fs_inode = finode;
        node->size = size;
        node->type = tar_gettype(tar_file->typeflag);
        node_data[finode - NODE_LIST_BASE] = tar_file;

        // Get filename
        if(filename == NULL)
            // File is at root, no need to strip anything
            filename = tar_file->filename;

        if(*filename == '/')
            // Remove leading slash
            filename++;

        // Get parent path
        size_t parent_len = filename - tar_file->filename;
        char path_buf[parent_len];
        char* parent_path;

        if(parent_len == 0)
        {
            // Node is at root
            parent_path = "/";
            parent_len = 2;
        }
        else
        {
            // Node has a parent path, copy it
            parent_path = path_buf;
            memcpy(path_buf, tar_file->filename, parent_len);
            path_buf[parent_len - 1] = '\0';
        }

        // Append node to respective dirent list
        append_dirent(node, parent_path, filename, tar_file->filename);

        finode++;
        // Too many nodes? Stop parsing
        if(finode >= NUM_NODES + NODE_LIST_BASE)
        {
            klog_logln(LVL_WARN, "Too many entries in initrd");
            break;
        }

        // Move to next tar header (size / 512) + 1
        tar_file += ((size >> 9) + 1);
        if(size % 512)
            tar_file++;
    }

    return instance;
}
