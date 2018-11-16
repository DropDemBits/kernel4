#include <stdio.h>
#include <string.h>

#include <common/hal.h>
#include <common/io/uart.h>
#include <common/util/klog.h>
#include <common/mm/mm.h>

#define EARLY_SUBSYSTEM 0
#define EARLY_BUFFER_LEN 16384
#define MAX_BUFFER_SIZE 0x0C000000 // 64MiB

#define NUM_IDS 256
#define NUM_ID_BYTES (NUM_IDS / sizeof(uint32_t))

uint16_t early_index;
char early_klog_buffer[EARLY_BUFFER_LEN];

char* log_buffer = KNULL;       // The global log buffer
uint32_t log_index;             // Index into the log buffer
uint32_t allocated_limit;       // Last index before a new page has to be allocated

// Int to name mapping
uint32_t id_bitmap[NUM_ID_BYTES];
char* subsystem_names[NUM_IDS];

bool is_init = false;

static uint16_t get_id()
{
    for(uint16_t word_index = 0; word_index < NUM_ID_BYTES; word_index++)
    {
        if(id_bitmap[word_index] == 0xFFFFFFFF)
            continue;

        // TODO: Replace with optimized find first set bit asm method
        for(uint16_t bit = 0; bit < 32; bit++)
        {
            if(((id_bitmap[word_index] >> bit) & 1) == 0)
            {
                id_bitmap[word_index] |= (1 << bit);
                return (word_index << 5) + bit;
            }
        }
    }

    return 0;
}

static void put_id(uint16_t id)
{
    // Don't remove the core subsytem id
    if(id == 0)
        return;

    id_bitmap[id >> 5] &= ~(1 << (id & 0b11111));
}

void klog_early_init()
{
    // By the time this is called, we don't have any memory management facilities
    is_init = false;
    memset(early_klog_buffer, 0x00, EARLY_BUFFER_LEN);
    early_index = 0;
    subsystem_names[0] = "EARLY";
    klog_early_logln(INFO, "Early Logger Initialized");
}

void klog_init()
{
    // By this point, we should have a heap ready and be in the multitasking state
    memset(id_bitmap, 0, NUM_IDS / sizeof(uint32_t));
    memset(subsystem_names, 0, sizeof(subsystem_names));

    id_bitmap[0] = 0x1;
    subsystem_names[0] = "EARLY";

    log_buffer = (char*)(uintptr_t)get_klog_base();
    log_index = early_index;
    allocated_limit = EARLY_BUFFER_LEN + 4096;
    
    for(size_t i = 0; i < allocated_limit; i+= 0x1000)
    {
        mmu_map((uintptr_t)log_buffer + i, mm_alloc(1), MMU_FLAGS_DEFAULT);
    }

    // Clear buffer of old data
    memset(log_buffer, 0, allocated_limit);

    memcpy(log_buffer, early_klog_buffer, early_index);
    is_init = true;
    klog_logln(EARLY_SUBSYSTEM, INFO, "Main Logger Initialzed");
}

bool klog_is_init()
{
    return is_init;
}

char* klog_get_name(uint16_t id)
{
    if(id > NUM_IDS || subsystem_names[id] == NULL)
        return subsystem_names[0];
    return subsystem_names[id];
}

uint16_t klog_add_subsystem(char* name)
{
    uint16_t id = get_id();

    if(id != 0)
        subsystem_names[id] = name;
    
    return id;
}

void klog_early_log(enum klog_level level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    klog_early_logfv(level, 0, format, args);
    va_end(args);
}

void klog_early_logf(enum klog_level level, uint8_t flags, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    klog_early_logfv(level, flags, format, args);
    va_end(args);
}

void klog_early_logv(enum klog_level level, const char* format, va_list args)
{
    klog_early_logfv(level, 0, format, args);
}

void klog_early_logfv(enum klog_level level, uint8_t flags, const char* format, va_list args)
{
    if(EARLY_BUFFER_LEN - early_index < EARLY_BUFFER_LEN / 16)
        return;

    struct klog_entry* entry = (struct klog_entry*)(early_klog_buffer + early_index);
    entry->level = level;
    entry->flags = flags;
    entry->length = 0;
    entry->timestamp = 0;
    entry->subsystem_id = EARLY_SUBSYSTEM;

    entry->length += vsprintf(entry->data, format, args);

    early_index += sizeof(struct klog_entry) + entry->length;
}

void klog_early_logln(enum klog_level level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    klog_early_loglnv(level, format, args);
    va_end(args);
}

void klog_early_loglnf(enum klog_level level, uint8_t flags, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    klog_early_loglnfv(level, flags, format, args);
    va_end(args);
}

void klog_early_loglnv(enum klog_level level, const char* format, va_list args)
{
    klog_early_loglnfv(level, 0, format, args);
}

void klog_early_loglnfv(enum klog_level level, uint8_t flags, const char* format, va_list args)
{
    if(EARLY_BUFFER_LEN - early_index < EARLY_BUFFER_LEN / 16)
        return;
    
    struct klog_entry* entry = (struct klog_entry*)(early_klog_buffer + early_index);
    entry->level = level;
    entry->flags = flags;
    entry->timestamp = 0;
    entry->length = 0;
    entry->subsystem_id = EARLY_SUBSYSTEM;

    entry->length += vsprintf(entry->data, format, args);
    
    entry->data[entry->length] = '\n';
    entry->length++;

    early_index += sizeof(struct klog_entry) + entry->length;
}

void klog_early_logc(enum klog_level level, const char c)
{
    if(EARLY_BUFFER_LEN - early_index < EARLY_BUFFER_LEN / 16)
        return;
    
    struct klog_entry* entry = (struct klog_entry*)(early_klog_buffer + early_index);
    entry->level = level;
    entry->flags = KLOG_FLAG_NO_HEADER;
    entry->timestamp = 0;
    entry->length = 1;
    entry->subsystem_id = EARLY_SUBSYSTEM;

    entry->data[0] = c;

    early_index += sizeof(struct klog_entry) + entry->length;
}

// TODO: All of the methods below will fail if interrupted or re-entered. Add appropriate locks and atomic operations
static void check_alloc()
{
    if((allocated_limit - log_index) <= 4096)
    {
        mmu_map((uintptr_t)log_buffer + allocated_limit, mm_alloc(1), MMU_FLAGS_DEFAULT);
        memset(log_buffer + allocated_limit, 0, 4096);
        allocated_limit += 4096;
    }
}

void klog_log(uint16_t subsys_id, enum klog_level level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    klog_logfv(subsys_id, level, 0, format, args);
    va_end(args);
}

void klog_logln(uint16_t subsys_id, enum klog_level level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    klog_loglnfv(subsys_id, level, 0, format, args);
    va_end(args);
}

void klog_logf(uint16_t subsys_id, enum klog_level level, uint8_t flags, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    klog_logfv(subsys_id, level, flags, format, args);
    va_end(args);
}

void klog_loglnf(uint16_t subsys_id, enum klog_level level, uint8_t flags, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    klog_loglnfv(subsys_id, level, flags, format, args);
    va_end(args);
}

void klog_logv(uint16_t subsys_id, enum klog_level level, const char* format, va_list args)
{
    klog_logfv(subsys_id, level, 0, format, args);
}

void klog_loglnv(uint16_t subsys_id, enum klog_level level, const char* format, va_list args)
{
    klog_loglnfv(subsys_id, level, 0, format, args);
}

void klog_logfv(uint16_t subsys_id, enum klog_level level, uint8_t flags, const char* format, va_list args)
{
    struct klog_entry* entry = (struct klog_entry*)(log_buffer + log_index);
    entry->level = level;
    entry->flags = flags;
    entry->timestamp = timer_read_counter(0);
    entry->length = 0;
    entry->subsystem_id = subsys_id;

    entry->length += vsprintf(entry->data, format, args);

    log_index += sizeof(struct klog_entry) + entry->length;

    check_alloc();
}

void klog_loglnfv(uint16_t subsys_id, enum klog_level level, uint8_t flags, const char* format, va_list args)
{
    struct klog_entry* entry = (struct klog_entry*)(log_buffer + log_index);
    entry->level = level;
    entry->flags = flags;
    entry->timestamp = timer_read_counter(0);
    entry->length = 1;
    entry->subsystem_id = subsys_id;

    entry->length += vsprintf(entry->data, format, args);
    entry->data[entry->length - 1] = '\n';

    log_index += sizeof(struct klog_entry) + entry->length;

    check_alloc();
}

void klog_logc(uint16_t subsys_id, enum klog_level level, const char c)
{
    klog_logf(subsys_id, level, KLOG_FLAG_NO_HEADER, "%c", c);
}
