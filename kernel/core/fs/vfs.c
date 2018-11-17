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

#include <common/fs/vfs.h>
#include <common/mm/mm.h>

struct vfs_mount
{
    const char* path;
    vfs_inode_t* root;
    uint32_t is_root_node : 1;
    uint32_t path_len;
};

static vfs_inode_t *root_node = KNULL;
static const char *root_path = KNULL;
static unsigned int num_mounts;
static struct vfs_mount **vfs_mounts = KNULL;

void vfs_mount(vfs_inode_t* root_node, const char* path)
{
    bool is_root_node = false;
    if(strcmp(path, "/") == 0)
        is_root_node = true;

    if(vfs_mounts == KNULL)
        vfs_mounts = kcalloc(16, sizeof(struct vfs_mount*));
    else if(num_mounts > 16)
        vfs_mounts = krealloc(vfs_mounts, (16+num_mounts)*sizeof(struct vfs_mount*));

    vfs_mounts[num_mounts] = kmalloc(sizeof(struct vfs_mount));
    vfs_mounts[num_mounts]->path = path;
    vfs_mounts[num_mounts]->root = root_node;
    vfs_mounts[num_mounts]->is_root_node = is_root_node;
    vfs_mounts[num_mounts]->path_len = strlen(path);
    num_mounts++;
}

vfs_inode_t* vfs_getrootnode(const char* path)
{
    for(int i = num_mounts - 1; i > 0; i--)
        if(strncmp(path, vfs_mounts[i]->path, vfs_mounts[i]->path_len) == 0)
            return vfs_mounts[i]->root;

    return vfs_mounts[0]->root;
}

void vfs_setroot(vfs_inode_t* root_node, const char* path)
{
    root_node = root_node;
    root_path = path;
}

vfs_inode_t* vfs_getroot()
{
    return root_node;
}

ssize_t vfs_read(vfs_inode_t *file_node, size_t off, size_t len, uint8_t* buffer)
{
    if(file_node == KNULL)
        return -1;

    if(file_node->read != KNULL)
        return file_node->read(file_node, off, len, buffer);
    else
        return -1;
}

ssize_t vfs_write(vfs_inode_t *file_node, size_t off, size_t len, uint8_t* buffer)
{
    if(file_node == KNULL)
        return -1;

    if(file_node->write != KNULL)
        return file_node->write(file_node, off, len, buffer);
    else
        return -1;
}

void vfs_open(vfs_inode_t *file_node, int oflags)
{
    if(file_node == KNULL)
        return;

    if(file_node->open != KNULL)
        file_node->open(file_node, oflags);
}

void vfs_close(vfs_inode_t *file_node)
{
    if(root_node == KNULL)
        return;

    if(file_node->close != KNULL)
        file_node->close(file_node);
}

struct vfs_dirent* vfs_readdir(vfs_inode_t *root_node, size_t index)
{
    if(root_node == KNULL)
        return NULL;

    if((root_node->type & 0x7) == VFS_TYPE_DIRECTORY && root_node->readdir != KNULL)
        return root_node->readdir(root_node, index);
    else
        return NULL;
}

vfs_inode_t* vfs_finddir(vfs_inode_t *root_node, const char* name)
{
    if(root_node == KNULL)
        return NULL;

    if((root_node->type & 0x7) == VFS_TYPE_DIRECTORY && root_node->finddir != KNULL)
        return root_node->finddir(root_node, name);
    else
        return NULL;
}
