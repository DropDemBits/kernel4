#include <stdarg.h>
#include <common/types.h>

#ifndef __KLOG_H__
#define __KLOG_H__ 1

enum klog_level
{
    EOL = 0x0,
    DEBUG = 1,
    INFO,
    WARN,
    ERROR,
};

struct klog_entry
{
    uint8_t level;          // Log level used
    uint8_t flags;          // Flags, if any
    uint16_t length;        // Length of the data payload (excludes null terminator)
    uint16_t subsystem_id;  // Subsystem ID associated with this
    uint32_t align;
    uint64_t timestamp;     // Timestamp is zero until HAL is initialized
    char data[0];           // String is not null terminated
} __attribute__((__packed__));

/**
 * @brief  Initializes the bare minimum needed to get the kernel logger going 
 * @note   
 * @retval None
 */
void klog_early_init();

/**
 * @brief  Initializes the parts needed in order to function inside a multi-threaded environment
 * @note   
 * @retval None
 */
void klog_init();

// Early-init versions of the methods below
// All of these will use the subsytem id 0 for "EARLY"
void klog_early_log(enum klog_level level, char* format, ...);
void klog_early_logln(enum klog_level level, char* format, ...);
void klog_early_logc(enum klog_level level, char c);

/**
 * @brief  Adds a subsystem to the logger for naming
 * @note   Any unamed subsystem will get the subsytem name " UNK "
 * @param  name: The name of the subsytem
 * @retval The subsytem id allocated to the registered subsystem
 */
uint16_t klog_add_subsystem(char* name);
char* klog_get_name(uint16_t id);

void klog_log(uint16_t subsys_id, enum klog_level level, char* format, ...);
void klog_logv(uint16_t subsys_id, enum klog_level level, char* format, va_list args);
void klog_logln(uint16_t subsys_id, enum klog_level level, char* format, ...);
void klog_loglnv(uint16_t subsys_id, enum klog_level level, char* format, va_list args);
void klog_logc(uint16_t subsys_id, enum klog_level level, char c);

#endif