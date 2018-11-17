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
#include <common/util/klog.h>

#define ROOT_INODE 1
#define NODE_LIST_BASE (ROOT_INODE + 1)
#define NUM_NODES   64
#define NUM_DIRS    64
#define NUM_PREALLOC_DIRENTS 16

/*
typedef struct vfs_inode {
    // name in dirent
    uint32_t perms_mask : 12; // Access permissions
    uint32_t type : 4; // Node type
    uint32_t uid;    // User ID
    uint32_t gid;    // Group ID
    uint32_t size;    // Size in bytes
    ino_t fs_inode; // Associated fs inode
    vfs_read_func_t read;
    vfs_write_func_t write;
    vfs_open_func_t open;
    vfs_close_func_t close;
    vfs_readdir_func_t readdir;
    vfs_finddir_func_t finddir;
    uint32_t impl_specific; // VFS-impl specific value (dnode)
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

struct tar_dir
{
    char* base_path;            // Base Path of directory
    struct vfs_dirent* dirents; // Directory entries for subdirs
    size_t num_dirents;         // Number of dirents
    ino_t real_inode;           // Inode of associated dir
};

static vfs_inode_t* root_node;
static vfs_inode_t* node_list[NUM_NODES];
static struct tar_header *node_data[NUM_NODES];
static size_t num_nodes = 0;

static struct tar_dir node_dirs[NUM_DIRS];
static size_t num_dirs = 0;

static const char* strrchr(const char* str, int chr)
{
    size_t len = strlen(str);
    char* index = str + len;

    if(!chr)
        return index;

    // Skip null byte
    len--;
    index--;

    // Search for char
    while(len--)
    {
        if(*index == (char)chr)
            return index;
        index--;
    }

    return NULL;
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

static struct tar_dir* find_dir(char* path)
{
    for(size_t i = 0; i < num_dirs; i++)
    {
        if(strncmp(node_dirs[i].base_path, path, strlen(path)) != 0)
            continue;

        return &node_dirs[i];
    }

    return NULL;
}

static size_t get_parent_len(char* path)
{
    char* current_dir = strrchr(path, '/');
    if(current_dir != NULL && strlen(current_dir) > 1)
        return current_dir - path + 1;
    else
        return 2;   // strlen("/")
}

static ssize_t tarfs_read(vfs_inode_t *node, size_t off, size_t len, uint8_t* buffer)
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
        buffer[read_len] = data[off+read_len];
    }

    return read_len;
}

static void tarfs_open(vfs_inode_t* file_node, int oflags)
{
    file_node->num_users++;
}

static void tarfs_close(vfs_inode_t* file_node)
{
    file_node->num_users--;
}

static struct vfs_dirent* tarfs_readdir(vfs_inode_t *node, size_t index)
{
    struct tar_dir* dir = &node_dirs[node->impl_specific];
    if(index >= dir->num_dirents)
        return NULL;
    return &dir->dirents[index];
}

static vfs_inode_t* tarfs_finddir(vfs_inode_t *node, const char* name)
{
    struct tar_dir* base_dir;
    char* filename = name;

    // Rebase to actual path if the path is absolute
    if(*name == '/')
    {
        size_t parent_len = get_parent_len(name);
        char parent_path[parent_len];

        memcpy(parent_path, name, parent_len);
        parent_path[parent_len - 1] = '\0';
        base_dir = find_dir(parent_path);
    }
    else if(strrchr(name, '/') != NULL)
    {
        // Name is relative to the node, but is in a subdirectory of the node
        char* base_path = node_dirs[node->impl_specific].base_path;
        size_t subdir_len = get_parent_len(name);
        size_t combined_len = strlen(base_path) + subdir_len;
        char real_path[combined_len];

        // Combine paths
        memset(real_path, 0, combined_len);
        strcpy(real_path, base_path);
        strncat(real_path, name, subdir_len);

        filename = strrchr(name, '/');
        filename++;

        // TODO: Test this path out
        klog_logln(1, DEBUG, "Bp: %s, Fn: %s (%s/%s)", real_path, filename, real_path, filename);
        return NULL;
    }
    else
    {
        // Name is relative to the node and a direct child
        base_dir = &node_dirs[node->impl_specific];
    }

    // Search for filename in dirents
    for(size_t i = 0; i < base_dir->num_dirents; i++)
    {
        if(strcmp(name, base_dir->dirents[i].name) == 0)
        {
            if(base_dir->dirents[i].inode >= NODE_LIST_BASE)
                return node_list[base_dir->dirents[i].inode - NODE_LIST_BASE];
            else if(base_dir->dirents[i].inode == ROOT_INODE)
                return root_node;

            // Unknown case, log it
            klog_logln(1, ERROR, "Unable to resolve path: %s/%s (%d)", base_dir->dirents[i].name, name, base_dir->dirents[i].inode);
            return NULL;
        }
    }

    klog_logln(1, DEBUG, "Oddie?: %p, %s, %s", node, name, base_dir->base_path);
    return NULL;
}

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

static void construct_node(vfs_inode_t* node)
{
    if(node == NULL)
        return;

    memset(node, 0, sizeof(vfs_inode_t));
    node->symlink_ptr = KNULL;
    node->read = tarfs_read;
    node->write = KNULL;
    node->open = KNULL;
    node->close = KNULL;
    node->readdir = tarfs_readdir;
    node->finddir = tarfs_finddir;
    num_nodes++;
}

static void construct_dir_node(struct tar_dir* dnode, vfs_inode_t* inode, char* base_path, ino_t dinode)
{
    dnode->base_path = base_path;
    dnode->dirents = NULL;
    dnode->num_dirents = 0;
    dnode->real_inode = inode->fs_inode;
    inode->impl_specific = dinode;
    num_dirs++;
}

/*
 * All base_path's must be absolute paths (excluding the initial /)
 */
static void append_dirent(vfs_inode_t* target, char* base_path, char* name)
{
    struct tar_dir* dirnode = find_dir(base_path);
    if(!dirnode)
    {
        klog_logln(1, ERROR, "initrd dirnode %s does not exist (yet?)", base_path);
        return;
    }

    // Build dir list
    if(dirnode->num_dirents % NUM_PREALLOC_DIRENTS == 0)
        dirnode->dirents = krealloc(dirnode->dirents, (dirnode->num_dirents + NUM_PREALLOC_DIRENTS) * sizeof(struct vfs_dirent));

    if(dirnode->num_dirents == 0)
    {
        // . and .. dirents have not been setup yet, setup those nodes
        struct vfs_dirent* dirent = &dirnode->dirents[dirnode->num_dirents++];
        char* parent_base;
        size_t parent_len = get_parent_len(base_path);
        struct tar_dir* parent = NULL;
        char parent_path[parent_len];

        if(parent_len == 2)
            parent_base = "/";
        else
            parent_base = base_path;

        memcpy(parent_path, parent_base, parent_len);
        parent_path[parent_len - 1] = '\0';
        parent = find_dir(parent_path);

        // If there's no parent, it's the root (loops back on itself)
        if(!parent)
            parent = dirnode;

        // . dirent
        dirent->inode = dirnode->real_inode;
        dirent->name = ".";

        // .. dirent
        dirent = &dirnode->dirents[dirnode->num_dirents++];
        dirent->inode = parent->real_inode;
        dirent->name = "..";
    }

    // Setup current dirent
    struct vfs_dirent* dirent = &dirnode->dirents[dirnode->num_dirents++];
    dirent->inode = target->fs_inode;
    dirent->name = name;
}

vfs_inode_t* tarfs_init(void* address, size_t tar_len)
{
    if(address == KNULL) return KNULL;

    // Identity map tarfs into address space (Temporary)
    for(size_t pages = 0; pages < ((tar_len + 0xFFF) >> 12); pages++)
    {
        mmu_map((uintptr_t)address + (pages << 12), (uintptr_t)address + (pages << 12), MMU_FLAGS_DEFAULT);
    }

    struct tar_header *tar_file = (struct tar_header*) address;
    if(strncmp(tar_file->ustar_magic,"ustar",6) != 0)
        return KNULL;

    // Setup root node
    root_node = kmalloc(sizeof(vfs_inode_t));
    construct_node(root_node);
    root_node->type = VFS_TYPE_DIRECTORY;
    root_node->fs_inode = ROOT_INODE;
    root_node->impl_specific = 0;

    // Setup root dirnode
    construct_dir_node(&node_dirs[0], root_node, "/", 0);

    int finode = NODE_LIST_BASE;
    int dnode = 1;
    while(tar_file->filename[0])
    {
        unsigned int size = getsize(tar_file->size);
        char* filename = strrchr(tar_file->filename, '/');
        vfs_inode_t* node = kmalloc(sizeof(vfs_inode_t));

        // Setup vfs_node
        node_list[finode - NODE_LIST_BASE] = node;
        construct_node(node);
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
            goto node_at_root;
        }

        // Node has a parent path, copy it
        parent_path = path_buf;
        memcpy(path_buf, tar_file->filename, parent_len);
        path_buf[parent_len - 1] = '\0';

    node_at_root:
        // Append node to respective dirent list
        append_dirent(node, parent_path, filename);

        // Construct dirnode for node dirs
        if(node->type == VFS_TYPE_DIRECTORY)
        {
            // Copy dir path to heap
            char* base_path = kmalloc(strlen(tar_file->filename));
            memcpy(base_path, tar_file->filename, strlen(tar_file->filename));

            construct_dir_node(&node_dirs[dnode], node, base_path, dnode);
            dnode++;
        }

        finode++;
        // Too many nodes? Stop parsing
        if(finode >= NUM_NODES + NODE_LIST_BASE)
        {
            klog_logln(1, WARN, "Too many entries in initrd");
            break;
        }

        // Move to next tar header (size / 512) + 1
        tar_file += ((size >> 9) + 1);
        if(size % 512)
            tar_file++;
    }

    return root_node;
}
