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

static struct inode *root_node = KNULL;
static unsigned int num_mounts;
static struct vfs_mount **vfs_mounts = KNULL;

static struct vfs_mount *root_mount = KNULL;

struct dnode* vfs_walk_path(struct dnode* base_dir, const char* path)
{
    struct dnode* dnode = base_dir;

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
        {
            // Check for last slash on a non-dir
            if((to_inode(dnode)->type & 0x7) != VFS_TYPE_DIRECTORY && *(path_node - 1) == '/')
                return NULL;

            // Done with it
            break;
        }

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
                if(dnode->parent != NULL)
                    // Rebase to parent
                    dnode = dnode->parent;

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

                // Skip path lookup, move to next component
                continue;
            }
        }
        else
        {
            // Copy path component
            memcpy(current_node, path_node, len+1);
            current_node += len+1;
        }

        *current_node = '\0';

        klog_logln(LVL_DEBUG, "lkup: %s", current_node - len - 1);
        // Lookup & rebase
        dnode = dnode->dops->find_dir(dnode, current_node - len - 1);

        if(dnode == NULL)
            return NULL;

        if((dnode->inode->type & VFS_TYPE_MOUNT) != 0)
        {
            // We have found a mount point, jump to it
            dnode = dnode->inode->symlink_ptr->instance->root;
        }

    } while(true);

    return dnode;
}

void vfs_mount(struct fs_instance* root, const char* path)
{
    struct vfs_mount* mount = kmalloc(sizeof(struct vfs_mount));
    mount->instance = root;
    mount->path = path;
    mount->next = NULL;
    num_mounts++;

    if(root_mount == KNULL)
    {
        // Make this mount the new root
        root_mount = mount;
    }
    else
    {
        // Go through & attach it to the correct node
        struct dnode *parent_root = vfs_walk_path(root_mount->instance->root, path);

        if(parent_root == NULL)
        {
            // Directory is non-existant
            klog_logln(LVL_ERROR, "Unable to mount fs to %s (directory does not exist)", path);
            kfree(mount);
            return;
        }

        // Attach inode to root of new fs
        parent_root->inode->symlink_ptr = root->root->inode;
        parent_root->inode->type |= VFS_TYPE_MOUNT;

        // Link root back up
        root->root->parent = parent_root;

        // Add to the list of mounts
        mount->next = root_mount->next;
        root_mount->next = mount;

        klog_logln(LVL_DEBUG, "Mounted fs to %s", path);
    }
}

struct vfs_mount* vfs_get_mount(const char* path)
{
    if(strcmp(path, "/") == 0)
        // Quick exit
        return root_mount;

    // Search through all the mounts for a path
    for(struct vfs_mount* mount = root_mount->next; mount != NULL; mount = mount->next)
    {
        if(strcmp(mount->path, path) == 0)
            return mount;
    }

    // None found
    return NULL;
}

ssize_t vfs_read(struct inode *file_node, size_t off, size_t len, void* buffer)
{
    if(file_node == KNULL)
        return -1;

    if(file_node->iops->read != NULL)
        return file_node->iops->read(file_node, off, len, buffer);
    else
        return -1;
}

ssize_t vfs_write(struct inode *file_node, size_t off, size_t len, void* buffer)
{
    if(file_node == KNULL)
        return -1;

    if(file_node->iops->write != NULL)
        return file_node->iops->write(file_node, off, len, buffer);
    else
        return -1;
}

void vfs_open(struct inode *file_node, int oflags)
{
    if(file_node == KNULL)
        return;

    if(file_node->iops->open != NULL)
        file_node->iops->open(file_node, oflags);
}

void vfs_close(struct inode *file_node)
{
    if(root_node == KNULL)
        return;

    if(file_node->iops->close != NULL)
        file_node->iops->close(file_node);
}

struct dirent* vfs_readdir(struct dnode *dnode, size_t index, struct dirent* dirent)
{
    if(dnode == KNULL)
        return NULL;

    if(dnode->dops->read_dir != NULL)
        return dnode->dops->read_dir(dnode, index, dirent);
    else
        return NULL;
}

struct dnode* vfs_find_dir(struct dnode *root, const char* path)
{
    if(root == NULL || root == NULL)
        return NULL;
    return vfs_walk_path(root, path);
}

struct inode* to_inode(struct dnode* dnode)
{
    return dnode->inode;
}
