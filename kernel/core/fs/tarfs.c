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
    struct dirent* dirents; // Directory entries for subdirs
    size_t num_dirents;         // Number of dirents
    ino_t real_inode;           // Inode of associated dir
};

static struct vfs_dir* root_dir;
static vfs_inode_t* root_node;
static vfs_inode_t* node_list[NUM_NODES];
static struct tar_header *node_data[NUM_NODES];
static size_t num_nodes = 0;

static struct tar_dir node_dirs[NUM_DIRS];
static size_t num_dirs = 0;

static unsigned int getsize(const char *in)
{
    unsigned int size = 0;
    unsigned int j;
    unsigned int count = 1;

    for (j = 11; j > 0; j--, count *= 8)
        size += ((in[j - 1] - '0') * count);

    return size;
}

static vfs_inode_t* get_inode(struct vfs_fs_instance *instance, ino_t inode)
{
    if(inode == 0)
        return NULL;        // Bad inode
    else if(inode == ROOT_INODE)
        return root_node;   // Get the root inode
    
    return node_list[inode - NODE_LIST_BASE];
}

static struct vfs_dir* find_dir(struct vfs_dir *base, char* path)
{
    if(base == NULL || path == NULL)
        return NULL;

    // Walk through all dirs, starting at the base dir
    char path_components[MAX_PATHLEN];
    char *component = NULL, *last = NULL;
    struct vfs_dir* dir = base;

    memcpy(path_components, path, strlen(path));
    path_components[strlen(path)] = '\0';

    // Check if root is the target dir
    if(strcmp(path, dir->name) == 0)
        return dir;

    component = strtok_r(path_components, "/", &last);

    // Root isn't target dir, so continue
    do
    {
        bool found_path = false;

        for(struct vfs_dir *child = dir->subdirs; child != NULL; child = child->next)
        {
            if(strcmp(child->name, component) == 0)
            {
                dir = child;
                found_path = true;
                break;
            }
        }

        if(found_path)
            continue;

        // No match found, so exit
        dir = NULL;
        break;
    } while((component = strtok_r(NULL, "/", &last)) != NULL);

    return dir;
}

/**
 * @brief  Gets the length of the parent path (excluding the null byte)
 * @note   Also includes the path seperator (if there is any)
 * @param  path: The path to get the parent path from
 * @retval The length of the parent path
 */
static size_t get_parent_len(char* path)
{
    // TODO: Solve issue of traling slash (i.e /blorg/aaa*/*)
    const char* current_dir = strrchr(path, '/');
    if(current_dir != NULL && strlen(current_dir) > 1)
        return current_dir - path;
    else
        return 0;   // strlen("/")
}

static ssize_t tarfs_read(vfs_inode_t *node, size_t off, size_t len, void* buffer)
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

static void tarfs_open(vfs_inode_t* file_node, int oflags)
{
    file_node->num_users++;
}

static void tarfs_close(vfs_inode_t* file_node)
{
    file_node->num_users--;
}

static struct vfs_inode_ops iops = 
{
    .read = tarfs_read,
    .write = NULL,
    .open = NULL,
    .close = NULL,
};

static struct dirent* tarfs_readdir(struct vfs_dir *dnode, size_t index, struct dirent* dirent)
{
    // Don't bother for dnode that isn't a directory
    if((get_inode(dnode->instance, dnode->inode)->type & 7) != VFS_TYPE_DIRECTORY)
        return NULL;

    // Index 0 & 1 are "." and "..", 2 are the actual directories
    switch(index)
    {
        case 0:
            dirent->inode = dnode->inode;
            dirent->name = ".";
            goto finished;
        case 1:
            if(dnode->parent != NULL)
                dirent->inode = dnode->parent->inode;
            else
                dirent->inode = dnode->inode;
            dirent->name = "..";
            goto finished;
    };

    // Go through all of the directories
    struct vfs_dir* child = dnode->subdirs;

    // Base at zero
    index -= 2;

    // Keep going until the index is zero, or we hit the end
    for(; child != NULL && index; index--, child = child->next);

    // By now, we should have the child node

    // Reached the end of the directory list
    if(child == NULL)
        return NULL;

    // Fill up dirent
    dirent->inode = child->inode;
    dirent->name = child->name;

    finished:
    return dirent;
}

static struct vfs_dir* tarfs_finddir(struct vfs_dir *dnode, const char* path)
{
    if(strcmp(path, ".") == 0)
        return dnode;
    else if(strcmp(path, "..") == 0)
    {
        if(dnode->parent != NULL)
            return dnode->parent;

        // Went up to the root
        return dnode;
    }

    // Find the path now
    struct vfs_dir *dir = find_dir(dnode, path);

    if(!dir)
        return NULL;

    return dir;
}

static struct vfs_dnode_ops dops = 
{
    .read_dir = tarfs_readdir,
    .find_dir = tarfs_finddir,
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

static void construct_node(vfs_inode_t* node)
{
    if(node == NULL)
        return;

    memset(node, 0, sizeof(vfs_inode_t));
    node->symlink_ptr = NULL;
    node->iops = &iops;
    num_nodes++;
}

static void construct_dir_node(struct vfs_dir* dnode, vfs_inode_t* inode, const char* name, const char* path)
{
    dnode->path = path;
    dnode->name = name;
    dnode->inode = inode->fs_inode;
    dnode->subdirs = NULL;
    dnode->next = NULL;
    dnode->parent = NULL;

    // Replace w/ dops
    dnode->dops = &dops;
}

/*
 * All base_path's must be absolute paths (excluding the initial /)
 */
static void append_dirent(vfs_inode_t* target, char* base_path, const char* filename, const char* path)
{
    struct vfs_dir* parent = find_dir(root_dir, base_path);
    if(!parent)
    {
        klog_logln(ERROR, "initrd dirnode %s does not exist (yet?)", base_path);
        return;
    }

    // Setup current dirent
    struct vfs_dir* dnode = kmalloc(sizeof(struct vfs_dir));
    construct_dir_node(dnode, target, filename, path);

    // Pass the fs_instance down
    dnode->instance = parent->instance;

    // Append to the parent
    dnode->next = parent->subdirs;
    parent->subdirs = dnode;
}

struct vfs_fs_instance* tarfs_init(void* address, size_t tar_len)
{
    if(address == KNULL) return KNULL;

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
    root_dir = kmalloc(sizeof(struct vfs_dir));
    construct_dir_node(root_dir, root_node, "/", "/");

    // Setup fs_instance
    struct vfs_fs_instance* instance = kmalloc(sizeof(struct vfs_fs_instance));
    instance->get_inode = get_inode;
    instance->root = root_dir;

    // Setup instance
    root_dir->instance = instance;

    int finode = NODE_LIST_BASE;
    while(tar_file->filename[0])
    {
        unsigned int size = getsize(tar_file->size);
        const char* filename = strrchr(tar_file->filename, '/');
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
        }
        else
        {
            // Node has a parent path, copy it
            parent_path = path_buf;
            memcpy(path_buf, tar_file->filename, parent_len);
            path_buf[parent_len - 1] = '\0';
        }

    node_at_root:
        // Append node to respective dirent list
        append_dirent(node, parent_path, filename, tar_file->filename);

        finode++;
        // Too many nodes? Stop parsing
        if(finode >= NUM_NODES + NODE_LIST_BASE)
        {
            klog_logln(WARN, "Too many entries in initrd");
            break;
        }

        // Move to next tar header (size / 512) + 1
        tar_file += ((size >> 9) + 1);
        if(size % 512)
            tar_file++;
    }

    return instance;
}
