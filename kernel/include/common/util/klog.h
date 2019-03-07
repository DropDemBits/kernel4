#include <stdarg.h>
#include <common/types.h>

#ifndef __KLOG_H__
#define __KLOG_H__ 1

#define KLOG_FLAG_NO_HEADER 0b00000001

enum klog_level
{
    EOL = 0x0,
    LVL_DEBUG = 1,
    LVL_INFO,
    LVL_WARN,
    LVL_ERROR,
    LVL_FATAL,
};

struct klog_entry
{
    uint8_t level;          // Log level used
    uint8_t flags;          // Flags, if any
    uint16_t length;        // Length of the data payload (excludes null terminator)
    //uint16_t padding;
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

bool klog_is_init();

void klog_log(enum klog_level level, const char* format, ...);
void klog_logv(enum klog_level level, const char* format, va_list args);
void klog_logln(enum klog_level level, const char* format, ...);
void klog_loglnv(enum klog_level level, const char* format, va_list args);
void klog_logf(enum klog_level level, uint8_t flags, const char* format, ...);
void klog_logfv(enum klog_level level, uint8_t flags, const char* format, va_list args);
void klog_loglnf(enum klog_level level, uint8_t flags, const char* format, ...);
void klog_loglnfv(enum klog_level level, uint8_t flags, const char* format, va_list args);
void klog_logc(enum klog_level level, const char c);

#endif