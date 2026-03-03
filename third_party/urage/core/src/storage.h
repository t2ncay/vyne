#ifndef STORAGE_H
#define STORAGE_H

#include "constants.h"
#include "pager.h"

typedef struct {
    Pager *pager;
    offset_t next_offset;
} Storage;

Storage *storage_create(Pager *pager);
DB_Result storage_write(Storage *storage, const void *data, size_t size, page_num_t *page, offset_t *offset);
DB_Result storage_read(Storage *storage, page_num_t page, offset_t offset, void *buffer, size_t *size);
DB_Result storage_delete(Storage *storage, page_num_t page, offset_t offset);

#endif