#include <common/mm/liballoc.h>
#include <common/fs/vfs.h>
#include <common/fs/filedesc.h>
#include <common/errno.h>
#include <string.h>

#include <common/util/klog.h>

#define BITMAP_BITS 64

// Bitmap management
// Create the bitmap, with initial size
// Initial size must be a multiple of 64
int bitmap_init(struct bitmap* bitmap, size_t initial_size)
{
    if (!bitmap)
        return;

    // If no inital size, allocate at least 4*64 bit entries
    if (!initial_size)
        initial_size = 4;

    // Round up initial size
    if (initial_size & 0x3F)
        initial_size = (initial_size + 0x3F) & ~0x3F;

    bitmap->bitmaps = kmalloc((initial_size / BITMAP_BITS) * sizeof(uint64_t));

    if (!bitmap->bitmaps)
         // TODO: Report allocation failure
        return -ENOMEM;
    
    bitmap->bitmap_len = initial_size / BITMAP_BITS;

    memset(bitmap->bitmaps, 0, bitmap->bitmap_len * sizeof(uint64_t));

    return 0;
}

// Grows the bitmap by "grow_by" entries
int bitmap_grow(struct bitmap* bitmap, size_t grow_by)
{
    if (!bitmap)
        return;
    if (!grow_by)
        return;
    if (grow_by & 0x3F)
        grow_by = (grow_by + 0x3F) & ~0x3F;

    uint64_t* old_map = bitmap->bitmaps;
    size_t old_len = bitmap->bitmap_len;

    size_t grow_words = grow_by / BITMAP_BITS;
    size_t new_size = bitmap->bitmap_len + grow_words;
    bitmap->bitmaps = krealloc(bitmap->bitmaps, new_size * sizeof(uint64_t));
    if (!bitmap->bitmaps)
        return -ENOMEM;

    // Zero new memory

    klog_logln(LVL_DEBUG, "grow! (%p -> %p) [%d -> %d] (%p)", old_map, bitmap->bitmaps, old_len * sizeof(uint64_t), new_size * sizeof(uint64_t), (bitmap->bitmaps + old_len));
    
    memset((bitmap->bitmaps + old_len), 0, grow_words * sizeof(uint64_t));
    bitmap->bitmap_len = new_size;

    return 0;
}

// Test a bit in the bitmap
// Returns -1 if the index was outside the bitmap, otherwise, the value reflecting the bit at the given bit index
int bitmap_test(struct bitmap* bitmap, size_t index)
{
    // No bitmap? no bits
    if (!bitmap)
        return -1;
    // No bitmap allocated? no bits
    if (!bitmap->bitmaps)
        return -1;

    size_t word_idx = index >> 6;
    size_t bit_idx = index & 0x3F;

    if (word_idx >= bitmap->bitmap_len)
        return -1;

    uint64_t word_bits = bitmap->bitmaps[word_idx];
    return (word_bits >> bit_idx) & 1;
}

void bitmap_set(struct bitmap* bitmap, size_t index)
{
    // No bitmap? no bits
    if (!bitmap)
        return;
    // No bitmap allocated? no bits
    if (!bitmap->bitmaps)
        return;

    size_t word_idx = index >> 6;
    size_t bit_idx = index & 0x3F;

    if (word_idx >= bitmap->bitmap_len)
        return;

    uint64_t* preserve = bitmap->bitmaps + word_idx;
    bitmap->bitmaps[word_idx] |= (1ULL << bit_idx);

    //klog_logln(LVL_DEBUG, "Set %d (%d): %016llx -> %016llx", index, word_idx, preserve, bitmap->bitmaps[word_idx]);
}

void bitmap_clear(struct bitmap* bitmap, size_t index)
{
    // No bitmap? no bits
    if (!bitmap)
        return;
    // No bitmap allocated? no bits
    if (!bitmap->bitmaps)
        return;

    size_t word_idx = index >> 6;
    size_t bit_idx = index & 0x3F;

    if (word_idx >= bitmap->bitmap_len)
        return;

    bitmap->bitmaps[word_idx] &= ~(1ULL << bit_idx);
}

/// File descriptor management
// Maximum number of files open
#define MAX_FD 65536
// Number of entries to grow by
#define GROW_BY 256

// Add an entry to the fd_map
// Assumes "fd_lock" has already been acquired
static int add_fd_entry(process_t* process, size_t fd, struct filedesc* filedesc)
{
    if (process->fd_map_len <= fd)
    {
        // Allocate a new fd map, adding 256 entries
        void* oldp = process->fd_map;
        size_t oldsize = process->fd_map_len;

        process->fd_map = krealloc(process->fd_map, sizeof(*process->fd_map) * (GROW_BY + process->fd_map_len));
        if (!process->fd_map)
            return -ENOMEM;

        volatile struct filedesc* touch_and_fail = process->fd_map[0];
        //*touch_and_fail = *touch_and_fail;
        klog_logln(LVL_DEBUG, "Zero [%p -> %p] (%d -> %d)", oldp, process->fd_map, oldsize * sizeof(*process->fd_map), (process->fd_map_len + GROW_BY) * sizeof(*process->fd_map));

        // Zero new memory
        memset(process->fd_map + process->fd_map_len, 0, sizeof(*process->fd_map) * GROW_BY);

        process->fd_map_len += GROW_BY;
    }

    process->fd_map[fd] = filedesc;

    if (bitmap_test(&process->fd_alloc, fd) == -1)
        bitmap_grow(&process->fd_alloc, GROW_BY);
    bitmap_set(&process->fd_alloc, fd);
    return 0;
}

// Frees the file descriptor number
// Assumes "fd_lock" has already been acquired
// Gives back the old "filedesc" structure
static struct filedesc* remove_fd_entry(process_t* process, size_t fd)
{
    // Not alloc'd? Not an fd
    if (bitmap_test(&process->fd_alloc, fd) != 1)
        return NULL;

    // Free the file descriptor
    bitmap_clear(&process->fd_alloc, fd);

    struct filedesc* filedesc = process->fd_map[fd];
    process->fd_map[fd] = NULL;
    return filedesc;
}

// Creates a file descriptor, returns the "fd" index
int create_filedesc(struct dnode* backing_dnode)
{
    if (!backing_dnode)
        return -ENOENT;

    int error_code = 0;

    process_t* current_process = sched_active_process();
    struct bitmap* fd_alloc = &current_process->fd_alloc;

    mutex_acquire(current_process->fd_lock);

    if (!fd_alloc->bitmaps)
    {
        klog_logln(LVL_DEBUG, "Bitmap make");
        // Initialliy allocate space for "GROW_BY" descriptors
        if (bitmap_init(fd_alloc, GROW_BY))
        {
            error_code = -ENOMEM;
            goto has_error;
        }
    }

    // Look for a free file descriptor index
    size_t use_fd = 0;
    for (;;)
    {
        int test_result = bitmap_test(fd_alloc, use_fd);

        //klog_logln(LVL_DEBUG, "Testing %d (%d)", use_fd, test_result);
        if (test_result <= 0)
        {
            // Bit is free, use that
            break;
        }

        use_fd++;
        if (use_fd >= MAX_FD)
        {
            error_code = -ENFILE;
            goto has_error;
        }
    }

    // Allocate a new file descriptor
    klog_logln(LVL_DEBUG, "Creating fd for %s (%d)", backing_dnode->name, use_fd);

    struct filedesc* filedesc = kmalloc(sizeof(*filedesc));
    if (!filedesc)
    {
        error_code = -ENOMEM;
        goto has_error;
    }

    filedesc->backing_dnode = backing_dnode;

    // Add to the map
    if(error_code = add_fd_entry(current_process, use_fd, filedesc))
        goto has_error_and_free;

    // Done, relinquish lock
    mutex_release(current_process->fd_lock);
    return use_fd;

    ///////////////////////////////////////////////////////

    has_error_and_free:
    kfree(filedesc);
    has_error:
    mutex_release(current_process->fd_lock);
    return error_code;
}

// Gets the file descriptor associated with the "fd" index. Returns NULL if not found
struct filedesc* get_filedesc(int fd)
{
    if (fd < 0)
        return NULL;

    process_t* current_process = sched_active_process();
    struct filedesc* filedesc = NULL;

    mutex_acquire(current_process->fd_lock);

    // Outside of entry map? Most likely free
    if ((size_t)fd >= current_process->fd_map_len)
    {
        klog_logln(LVL_DEBUG, "Outside entry size (%d >= %d)", fd, current_process->fd_map_len);
        mutex_release(current_process->fd_lock);
        return NULL;
    }
    
    // Outside of allocated entries or not allocated? Most likely free
    int bit_test = bitmap_test(&current_process->fd_alloc, (size_t)fd);
    if (bit_test != 1)
    {
        klog_logln(LVL_DEBUG, "Outside bitmap alloc (%d)", bit_test);
        mutex_release(current_process->fd_lock);
        return NULL;
    }

    filedesc = current_process->fd_map[(size_t)fd];

    mutex_release(current_process->fd_lock);
    return filedesc;
}

// Frees the file descriptor and associated index
int free_filedesc(int fd)
{
    if (fd < 0)
        return -EBADF;

    process_t* process = sched_active_process();
    struct bitmap* fd_alloc = &process->fd_alloc;

    mutex_acquire(process->fd_lock);
    struct filedesc* filedesc = remove_fd_entry(process, (size_t)fd);
    mutex_release(process->fd_lock);

    if (filedesc == NULL)
        return -EBADF;

    // Done with the file descriptor
    kfree(filedesc);

    return 0;
}
