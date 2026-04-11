#include "pager.h"
#include "btree.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

Pager *pager_open(const char *filename) {
    Pager *pager = calloc(1, sizeof(Pager));  // calloc zeros everything
    if (!pager) return NULL;
    
    // Make sure pages array is NULL
    for (int i = 0; i < TABLE_MAX_PAGES; i++) {
        pager->pages[i] = NULL;  // Explicitly set to NULL
    }
    
    pager->file = fopen(filename, "rb+");
    if (!pager->file) {
        // File doesn't exist - create it
        pager->file = fopen(filename, "wb+");
        if (!pager->file) {
            free(pager);
            return NULL;
        }
    }
    
    pager->page_size = PAGE_SIZE;
    
    // Determine number of pages
    fseek(pager->file, 0, SEEK_END);
    long file_size = ftell(pager->file);
    pager->num_pages = file_size / PAGE_SIZE;
    
    if (file_size % PAGE_SIZE != 0) {
        // Corrupted file
        fclose(pager->file);
        free(pager);
        return NULL;
    }
    
    return pager;
}

void pager_close(Pager *pager) {
    if (!pager) return;
    
    // Flush all pages to disk
    for (int i = 0; i < TABLE_MAX_PAGES; i++) {
        if (pager->pages[i]) {
            pager_flush_page(pager, i);
            free(pager->pages[i]);
            pager->pages[i] = NULL;
        }
    }
    
    fclose(pager->file);
    free(pager);
}

void *pager_get_page(Pager *pager, page_num_t page_num) {
    if (page_num > TABLE_MAX_PAGES) return NULL;
    
    printf("Debug: pager_get_page(%u), num_pages=%u\n", page_num, pager->num_pages);
    
    // Cache miss - load from disk
    if (!pager->pages[page_num]) {
        printf("Debug: Loading page %u from disk\n", page_num);
        void *page = calloc(1, pager->page_size);
        if (!page) return NULL;
        
        // Seek to page location
        if (fseek(pager->file, page_num * pager->page_size, SEEK_SET) != 0) {
            free(page);
            return NULL;
        }
        
        // Read the page
        size_t bytes_read = fread(page, pager->page_size, 1, pager->file);
        if (bytes_read < 1 && !feof(pager->file)) {
            free(page);
            return NULL;
        }
        
        // ✅ Assign to cache AFTER reading
        pager->pages[page_num] = page;
        
        // Now we can safely access it
        printf("    📖 Loaded page %u from disk\n", page_num);
        if (page_num == 0) {
            LeafNode* leaf = (LeafNode*)pager->pages[page_num];
            printf("      Page 0 has %u cells\n", leaf->num_cells);
        }
        
        // If this was the last page, update count
        if (page_num >= pager->num_pages) {
            pager->num_pages = page_num + 1;
        }
        
        printf("Debug: Page %u loaded, first 16 bytes:\n", page_num);
        for (int i = 0; i < 16; i++) {
            printf("%02X ", ((unsigned char*)page)[i]);
        }
        printf("\n");
    } else {
        printf("Debug: Page %u found in cache\n", page_num);
    }
    
    return pager->pages[page_num];
}

DB_Result pager_flush_page(Pager *pager, page_num_t page_num) {
    if (!pager->pages[page_num]) return DB_SUCCESS;
    
    printf("    💾 Writing page %d to disk\n", page_num);
    
    // For page 0, show what's being written
    if (page_num == 0) {
        LeafNode* leaf = (LeafNode*)pager->pages[page_num];
        printf("      Page 0 has %u cells\n", leaf->num_cells);
        for (int i = 0; i < leaf->num_cells; i++) {
            printf("        Cell %d: key=%u, value=%u\n", 
                   i, leaf->keys[i], leaf->values[i]);
        }
    }
    
    // Seek to page location
    if (fseek(pager->file, page_num * pager->page_size, SEEK_SET) != 0) {
        return DB_IO_ERROR;
    }
    
    // Write the page
    size_t bytes_written = fwrite(pager->pages[page_num], 
                                  pager->page_size, 1, pager->file);
    if (bytes_written < 1) {
        return DB_IO_ERROR;
    }
    
    return DB_SUCCESS;
}

DB_Result pager_flush_all(Pager *pager) {
    printf("  Pager flushing %u pages\n", pager->num_pages);
    
    for (int i = 0; i < TABLE_MAX_PAGES; i++) {
        if (pager->pages[i]) {
            printf("    Flushing page %d\n", i);
            pager_flush_page(pager, i);
        }
    }
    
    if (fflush(pager->file) != 0) {
        return DB_IO_ERROR;
    }
    
    return DB_SUCCESS;
}

page_num_t pager_allocate_page(Pager *pager) {
    // Find a free slot in memory
    for (int i = 0; i < TABLE_MAX_PAGES; i++) {
        if (!pager->pages[i]) {
            void *page = calloc(1, pager->page_size);
            if (!page) return INVALID_PAGE;
            pager->pages[i] = page;
            
            if (i >= pager->num_pages) {
                pager->num_pages = i + 1;
            }
            return i;
        }
    }
    return INVALID_PAGE;
}

