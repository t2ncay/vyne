#include "database.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Database *db_open(const char *db_name) {
    Database *db = malloc(sizeof(Database));
    if (!db) return NULL;
    
    db->name = strdup(db_name);
    
    // Construct filenames
    char index_filename[256];
    char data_filename[256];
    snprintf(index_filename, sizeof(index_filename), "%s.idx", db_name);
    snprintf(data_filename, sizeof(data_filename), "%s.dat", db_name);
    
    // Open index file
    Pager *index_pager = pager_open(index_filename);
    if (!index_pager) {
        free(db->name);
        free(db);
        return NULL;
    }
    
    // Open data file
    Pager *data_pager = pager_open(data_filename);
    if (!data_pager) {
        pager_close(index_pager);
        free(db->name);
        free(db);
        return NULL;
    }
    
    // Create index
    db->index = btree_create(index_pager);
    if (!db->index) {
        pager_close(index_pager);
        pager_close(data_pager);
        free(db->name);
        free(db);
        return NULL;
    }
    
    // Create storage
    db->storage = storage_create(data_pager);
    if (!db->storage) {
        // Cleanup
        free(db);
        return NULL;
    }
    
    return db;
}

DB_Result db_close(Database *db) {
    if (!db) return DB_ERROR;
    
    // Flush all changes
    pager_flush_all(db->index->pager);
    pager_flush_all(db->storage->pager);
    
    // Close pagers
    pager_close(db->index->pager);
    pager_close(db->storage->pager);
    
    free(db->index);
    free(db->storage);
    free(db->name);
    free(db);
    
    return DB_SUCCESS;
}

DB_Result db_insert(Database *db, uint32_t key, const void *data, size_t size) {
    printf("Debug: Inserting key %u with data size %zu\n", key, size);
    
    // Write data to storage
    page_num_t data_page;
    offset_t data_offset;
    DB_Result result = storage_write(db->storage, data, size, &data_page, &data_offset);
    if (result != DB_SUCCESS) {
        printf("Debug: storage_write failed with code %d\n", result);
        return result;
    }
    
    printf("Debug: Stored at page %u, offset %u\n", data_page, data_offset);
    
    // Pack page and offset into a single 32-bit value
    // Use 16 bits for page (max 65535) and 16 bits for offset (max 65535)
    uint32_t location = (data_page << 16) | (data_offset & 0xFFFF);
    
    printf("Debug: Packed location=%u (0x%X)\n", location, location);
    
    result = btree_insert(db->index, key, location);
    if (result != DB_SUCCESS) {
        printf("Debug: btree_insert failed with code %d\n", result);
    }
    
    return result;
}

DB_Result db_find(Database *db, uint32_t key, void *buffer, size_t *size) {
    printf("Debug: db_find(key=%u)\n", key);
    
    // Find in index
    page_num_t location;
    DB_Result result = btree_find(db->index, key, &location);
    if (result != DB_SUCCESS) {
        printf("Debug: btree_find failed with code %d\n", result);
        return result;
    }
    
    printf("Debug: btree_find returned location=%u (0x%X)\n", location, location);
    
    // Extract page and offset
    page_num_t data_page = location >> 16;  // Use 16 bits for page
    offset_t data_offset = location & 0xFFFF;  // Use 16 bits for offset
    
    printf("Debug: Extracted page=%u, offset=%u\n", data_page, data_offset);
    
    // Read from storage
    result = storage_read(db->storage, data_page, data_offset, buffer, size);
    printf("Debug: storage_read returned %d\n", result);
    
    return result;
}

DB_Result db_delete(Database *db, uint32_t key) {
    if (!db || !db->index) return DB_ERROR;
    
    printf("Debug: db_delete(key=%u)\n", key);
    
    // First find the key to get storage location (for cleanup)
    page_num_t location;
    DB_Result result = btree_find(db->index, key, &location);
    
    if (result == DB_SUCCESS) {
        // Extract storage location for potential cleanup
        page_num_t data_page = location >> 16;
        offset_t data_offset = location & 0xFFFF;
        printf("Debug: Found at page %u, offset %u - will mark as free\n", 
               data_page, data_offset);
        
        // In a real DB, you'd add this to a free list
        // storage_delete(db->storage, data_page, data_offset);
    }
    
    // Delete from index
    result = btree_delete(db->index, key);
    
    if (result == DB_SUCCESS) {
        printf("Deleted key %u\n", key);
    } else if (result == DB_NOT_FOUND) {
        printf("Key %u not found\n", key);
    } else {
        printf("Delete failed: %d\n", result);
    }
    
    return result;
}