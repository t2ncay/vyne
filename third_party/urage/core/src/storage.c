#include "storage.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t size;
    uint8_t data[];
} StorageRecord;

Storage *storage_create(Pager *pager) {
    Storage *storage = malloc(sizeof(Storage));
    if (!storage) return NULL;
    
    storage->pager = pager;
    storage->next_offset = 0;
    
    return storage;
}

DB_Result storage_write(Storage *storage, const void *data, size_t size, 
                        page_num_t *page, offset_t *offset) {
    // Use page 1 for data storage
    *page = 1;
    
    void *page_data = pager_get_page(storage->pager, *page);
    if (!page_data) return DB_ERROR;
    
    // Calculate sizes correctly
    uint32_t data_with_null = size + 1;  // Size of data including null terminator
    uint32_t total_size = sizeof(uint32_t) + data_with_null;  // Size prefix + data with null
    
    printf("Debug: size=%zu, data_with_null=%u, total_size=%u, next_offset=%u\n", 
           size, data_with_null, total_size, storage->next_offset);
    
    // Check if we have space
    if (storage->next_offset + total_size > PAGE_SIZE) {
        printf("Debug: Storage full! Need %u bytes, have %u\n", 
               total_size, PAGE_SIZE - storage->next_offset);
        return DB_FULL;
    }
    
    // Write size prefix (this is the size INCLUDING null terminator)
    memcpy((char *)page_data + storage->next_offset, &data_with_null, sizeof(uint32_t));
    
    // Write the data
    memcpy((char *)page_data + storage->next_offset + sizeof(uint32_t), data, size);
    
    // Add null terminator
    *((char *)page_data + storage->next_offset + sizeof(uint32_t) + size) = '\0';
    
    printf("Debug: Wrote size=%u at offset %u\n", data_with_null, storage->next_offset);
    printf("Debug: Data starts at offset %u\n", storage->next_offset + sizeof(uint32_t));
    
    *offset = storage->next_offset;
    storage->next_offset += total_size;
    
    return DB_SUCCESS;
}

DB_Result storage_read(Storage *storage, page_num_t page, offset_t offset, 
                       void *buffer, size_t *size) {
    void *page_data = pager_get_page(storage->pager, page);
    if (!page_data) return DB_ERROR;
    
    // Read size prefix (this is the size INCLUDING null terminator)
    uint32_t data_with_null;
    memcpy(&data_with_null, (char *)page_data + offset, sizeof(uint32_t));
    
    printf("Debug: Read size prefix=%u at offset %u\n", data_with_null, offset);
    
    // Validate the size (should be reasonable)
    if (data_with_null == 0 || data_with_null > PAGE_SIZE) {
        printf("Debug: Invalid size %u read from offset %u\n", data_with_null, offset);
        return DB_ERROR;
    }
    
    // Calculate actual data size (without null terminator)
    uint32_t data_size = data_with_null - 1;
    
    printf("Debug: Data size without null=%u\n", data_size);
    
    // Check if buffer is large enough
    if (*size < data_size) {
        *size = data_size;
        printf("Debug: Buffer too small, need %u bytes\n", data_size);
        return DB_ERROR;
    }
    
    // Read the data (excluding null terminator for now)
    memcpy(buffer, (char *)page_data + offset + sizeof(uint32_t), data_size);
    
    // Add null terminator
    ((char *)buffer)[data_size] = '\0';
    
    *size = data_size;
    
    printf("Debug: Successfully read %u bytes: '%s'\n", data_size, (char *)buffer);
    
    return DB_SUCCESS;
}

// Add this function to storage.c
DB_Result storage_delete(Storage *storage, page_num_t page, offset_t offset) {
    if (!storage || !storage->pager) return DB_ERROR;
    
    void *page_data = pager_get_page(storage->pager, page);
    if (!page_data) return DB_ERROR;
    
    // Read the size of the record (stored before the data)
    uint32_t size;
    memcpy(&size, (char *)page_data + offset, sizeof(uint32_t));
    
    // Add to free list (mark as available for reuse)
    // For simplicity, we'll just zero out the first few bytes to mark as deleted
    // A real implementation would maintain a proper free list
    uint32_t deleted_marker = 0xDEADBEEF;
    memcpy((char *)page_data + offset, &deleted_marker, sizeof(uint32_t));
    
    return DB_SUCCESS;
}