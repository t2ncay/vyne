#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 4096          // 4KB pages - standard filesystem block
#define TABLE_MAX_PAGES 100      // Max pages in memory
#define INVALID_PAGE UINT32_MAX

// Common return codes
typedef enum {
    DB_SUCCESS = 0,
    DB_ERROR = -1,
    DB_NOT_FOUND = -2,
    DB_FULL = -3,
    DB_IO_ERROR = -4,
    DB_MEMORY_ERROR = -5
} DB_Result;

// Common data types
typedef uint32_t page_num_t;
typedef uint64_t offset_t;

#endif