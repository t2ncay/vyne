#ifndef DATABASE_H
#define DATABASE_H

#include "constants.h"
#include "btree.h"
#include "storage.h"

typedef struct Database {
    BTree *index;
    Storage *storage;
    char *name;
    uint64_t total_keys;      // Add this
    uint64_t total_data_size;
} Database;


// Database operations
Database *db_open(const char *db_name);
DB_Result db_close(Database *db);
DB_Result db_insert(Database *db, uint32_t key, const void *data, size_t size);
DB_Result db_find(Database *db, uint32_t key, void *buffer, size_t *size);
DB_Result db_delete(Database *db, uint32_t key);

#endif