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

#include <common/fs/vfs.h>

#include <string.h>

#include <common/mm/mm.h>
#include <common/mm/liballoc.h>
#include <common/util/klog.h>

static vfs_inode_t *root_node = KNULL;
static const char *root_path = KNULL;
static unsigned int num_mounts;
static struct vfs_mount **vfs_mounts = KNULL;

struct vfs_dir* vfs_walk_path(struct vfs_dir* base_dir, const char* path)
{
    struct vfs_dir* dnode = base_dir;

    // All paths given are absolute

    // Walks the path given, returns the appropriate node
    char resolved_path[MAX_PATHLEN];
    char *current_node = resolved_path; // Points to current node in resolved path
    const char *path_node = path;  // Points to current path node
    size_t len = 0;

    // For every path seperator, do:
    // IF ., skip to next path seperator
    // IF .., delete currently pointed path
    // Find the directory
    //     IF it doesn't exist, stop immediately (ret NULL)

    resolved_path[0] = '\0';
    do
    {
        path_node += len;

        // Skip slashes
        while(*path_node == '/')
            path_node++;

        if(!*path_node)
            // Done with it
            break;

        // Get length of the path node
        const char *path_tail = strchr(path_node, '/');

        if(path_tail != NULL)
            len = (uintptr_t)(path_tail - path_node);
        else
            len = strlen(path_node);

        if(path_node[0] == '.')
        {
            if(path_node[1] == '.')
            {
                // Deal with going up
                if(strlen(resolved_path) == 0)
                    continue;
                
                // We are actually going up, so move back a node
                *(current_node - 1) = '\0';
                if(strchr(resolved_path, '/') == NULL)
                {
                    // Erase the entire path
                    current_node = resolved_path;
                    *current_node = '\0';
                }
                else
                {
                    // Erase the rest
                    current_node = strrchr(current_node, '/');
                    current_node++;
                }
            }
        }
        else
        {
            memcpy(current_node, path_node, len+1);
            current_node += len+1;
        }
        *current_node = '\0';
    } while(true);

    // Somehow, find this file/directory
    dnode = base_dir->dops->find_dir(base_dir, resolved_path);
    if(dnode == NULL)
        return NULL;

    return dnode;
}

void vfs_mount(struct vfs_fs_instance* root, const char* path)
{
    if(vfs_mounts == KNULL)
        vfs_mounts = kcalloc(16, sizeof(struct vfs_mount*));
    else if(num_mounts > 16)
        vfs_mounts = krealloc(vfs_mounts, (16+num_mounts)*sizeof(struct vfs_mount*));

    vfs_mounts[num_mounts] = kmalloc(sizeof(struct vfs_mount));
    vfs_mounts[num_mounts]->instance = root;
    vfs_mounts[num_mounts]->path = path;
    num_mounts++;
}

struct vfs_mount* vfs_get_mount(const char* path)
{
    for(int i = num_mounts - 1; i > 0; i--)
        if(strncmp(path, vfs_mounts[i]->path, strlen(vfs_mounts[i]->path)) == 0)
            return vfs_mounts[i];

    return vfs_mounts[0];
}

ssize_t vfs_read(vfs_inode_t *file_node, size_t off, size_t len, void* buffer)
{
    if(file_node == KNULL)
        return -1;

    if(file_node->iops->read != NULL)
        return file_node->iops->read(file_node, off, len, buffer);
    else
        return -1;
}

ssize_t vfs_write(vfs_inode_t *file_node, size_t off, size_t len, void* buffer)
{
    if(file_node == KNULL)
        return -1;

    if(file_node->iops->write != NULL)
        return file_node->iops->write(file_node, off, len, buffer);
    else
        return -1;
}

void vfs_open(vfs_inode_t *file_node, int oflags)
{
    if(file_node == KNULL)
        return;

    if(file_node->iops->open != NULL)
        file_node->iops->open(file_node, oflags);
}

void vfs_close(vfs_inode_t *file_node)
{
    if(root_node == KNULL)
        return;

    if(file_node->iops->close != NULL)
        file_node->iops->close(file_node);
}

struct dirent* vfs_readdir(struct vfs_dir *dnode, size_t index, struct dirent* dirent)
{
    if(dnode == KNULL)
        return NULL;

    if(dnode->dops->read_dir != NULL)
        return dnode->dops->read_dir(dnode, index, dirent);
    else
        return NULL;
}

struct vfs_dir* vfs_find_dir(struct vfs_dir *root, const char* path)
{
    if(root == NULL || root == NULL)
        return NULL;
    return vfs_walk_path(root, path);
}
