#ifndef PAGER_H
#define PAGER_H

#include "constants.h"
#include <stdio.h>

typedef struct Pager {
    FILE *file;
    void *pages[TABLE_MAX_PAGES];  // Page cache
    page_num_t num_pages;
    uint32_t page_size;
} Pager;

// Initialize and destroy
Pager *pager_open(const char *filename);
void pager_close(Pager *pager);

// Page operations
void *pager_get_page(Pager *pager, page_num_t page_num);
DB_Result pager_flush_page(Pager *pager, page_num_t page_num);
DB_Result pager_flush_all(Pager *pager);
page_num_t pager_allocate_page(Pager *pager);

#endif