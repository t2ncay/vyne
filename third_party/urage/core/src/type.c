#include "type.h"
#include "urage.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// System table prefix for storing types
#define TYPE_KEY_PREFIX "type:"

// Global type ID counter (in a real DB, this would be stored in system table)
static uint32_t next_type_id = 1;

// ==================== HELPER FUNCTIONS ====================

static char* trim_whitespace(char* str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    return str;
}

static int parse_field_definition(const char* input, FieldDef* field) {
    char field_name[64];
    char type_str[32];
    char size_str[32] = "";
    
    // Parse: name type or name type(size)
    int matched = sscanf(input, "%63s %31[^(](%31[^)])", field_name, type_str, size_str);
    if (matched < 2) {
        matched = sscanf(input, "%63s %31s", field_name, type_str);
    }
    
    if (matched < 2) return 0;
    
    strncpy(field->name, field_name, 31);
    field->name[31] = '\0';
    field->offset = 0;  // Will be set during size calculation
    
    if (strcmp(type_str, "int") == 0) {
        field->type = TYPE_INT;
        field->size = 4;  // int is 4 bytes
    } else if (strcmp(type_str, "string") == 0) {
        field->type = TYPE_STRING;
        if (strlen(size_str) > 0) {
            field->size = atoi(size_str);
        } else {
            field->size = 64;  // default string size
        }
    } else {
        return 0;  // unknown type
    }
    
    return 1;
}

// ==================== TYPE CREATION ====================

urage_result_t type_create(urage_db_t* db, const char* name, 
                           FieldDef* fields, uint32_t field_count) {
    if (!db || !name || !fields || field_count == 0) 
        return URAGE_INVALID_ARG;
    
    // Check if type already exists
    char type_key[256];
    snprintf(type_key, sizeof(type_key), "type:%s", name);
    
    char dummy[1];
    size_t size = 0;
    if (urage_get_str(db, type_key, dummy, &size) == URAGE_OK || size > 0) {
        return URAGE_ERROR;  // Type already exists
    }
    
    // Calculate total size (offsets already set by type_parse_fields)
    uint32_t total_size = 0;
    for (uint32_t i = 0; i < field_count; i++) {
        total_size += fields[i].size;
    }
    
    // Allocate memory for type definition
    size_t type_size = sizeof(TypeDef) + (field_count * sizeof(FieldDef));
    TypeDef* type = (TypeDef*)malloc(type_size);
    if (!type) return URAGE_MEMORY_ERROR;
    
    // Fill type definition
    static uint32_t next_type_id = 1;  // In real DB, this would be persistent
    type->id = next_type_id++;
    strncpy(type->name, name, 63);
    type->name[63] = '\0';
    type->size = total_size;
    type->field_count = field_count;
    memcpy(type->fields, fields, field_count * sizeof(FieldDef));
    
    // Store in database
    urage_result_t result = urage_put_str(db, type_key, type, type_size);
    
    free(type);
    return result;
}

// ==================== TYPE RETRIEVAL ====================

TypeDef* type_get(urage_db_t* db, const char* name) {
    if (!db || !name) return NULL;
    
    char type_key[256];
    snprintf(type_key, sizeof(type_key), "type:%s", name);
    
    // First, get the size needed
    size_t size = 0;
    urage_result_t result = urage_get_str(db, type_key, NULL, &size);
    
    // urage_get_str returns URAGE_ERROR but sets size when data exists
    if (size == 0) {
        printf("❌ Type key '%s' not found\n", type_key);
        return NULL;
    }
    
    printf("✅ Type key found, size=%zu bytes\n", size);
    
    // Allocate buffer for type
    TypeDef* type = (TypeDef*)malloc(size);
    if (!type) return NULL;
    
    // Get the actual data
    result = urage_get_str(db, type_key, type, &size);
    if (result != URAGE_OK) {
        printf("❌ Failed to get type data: %d\n", result);
        free(type);  // ← Make sure to free on error!
        return NULL;
    }
    
    return type;
}

// ==================== TYPE DELETION ====================

urage_result_t type_delete(urage_db_t* db, const char* name) {
    if (!db || !name) return URAGE_INVALID_ARG;
    
    char type_key[256];
    snprintf(type_key, sizeof(type_key), "%s%s", TYPE_KEY_PREFIX, name);
    
    // Delete the type definition
    return urage_del_str(db, type_key);
    
    // Note: In a real DB, you'd also delete all data of this type
    // This would require iterating over all keys with this prefix
}

// ==================== TYPE LISTING ====================

urage_result_t type_list(urage_db_t* db, char*** names, uint32_t* count) {
    if (!db || !names || !count) return URAGE_INVALID_ARG;
    
    // For now, we need a way to iterate over all keys
    // This is a simplified approach - in a real DB you'd have a cursor
    
    // We'll use a temporary approach: try common type IDs
    // In a real implementation, you'd maintain a separate index of type names
    
    *names = NULL;
    *count = 0;
    
    // Since we don't have iteration yet, we'll return a message
    // For now, users can use 'desc <typename>' to check specific types
    
    printf("ℹ️  Type listing requires cursor iteration (to be implemented)\n");
    printf("   For now, use 'desc <typename>' to check specific types\n");
    
    return URAGE_OK;
}

// ==================== FIELD PARSING ====================

urage_result_t type_parse_fields(const char* input, FieldDef** fields, 
                                  uint32_t* field_count) {
    if (!input || !fields || !field_count) return URAGE_INVALID_ARG;
    
    // Make a copy of the input
    char* input_copy = strdup(input);
    if (!input_copy) return URAGE_MEMORY_ERROR;
    
    // First, count the fields (pairs of name type)
    uint32_t count = 0;
    char* token = strtok(input_copy, " \t");
    while (token) {
        // Each field has a name AND a type (2 tokens)
        if (strlen(token) > 0) {
            count++;
        }
        token = strtok(NULL, " \t");
    }
    free(input_copy);
    
    // Must have even number of tokens (name, type pairs)
    if (count == 0 || count % 2 != 0) {
        return URAGE_INVALID_ARG;
    }
    
    *field_count = count / 2;
    *fields = (FieldDef*)calloc(*field_count, sizeof(FieldDef));
    if (!*fields) return URAGE_MEMORY_ERROR;
    
    // Now parse each field
    input_copy = strdup(input);
    if (!input_copy) {
        free(*fields);
        return URAGE_MEMORY_ERROR;
    }
    
    uint32_t field_idx = 0;
    char* name_token = strtok(input_copy, " \t");
    
    while (name_token && field_idx < *field_count) {
        // Get field name
        strncpy((*fields)[field_idx].name, name_token, 31);
        (*fields)[field_idx].name[31] = '\0';
        
        // Get field type (next token)
        char* type_token = strtok(NULL, " \t");
        if (!type_token) {
            free(input_copy);
            free(*fields);
            return URAGE_INVALID_ARG;
        }
        
        // Parse type (may include size like string(64))
        char type_str[32] = {0};
        char size_str[32] = {0};
        
        int parsed = sscanf(type_token, "%31[^(](%31[^)])", type_str, size_str);
        if (parsed < 1) {
            // Just copy the whole token as type
            strncpy(type_str, type_token, 31);
            type_str[31] = '\0';
        }
        
        if (strcmp(type_str, "int") == 0) {
            (*fields)[field_idx].type = TYPE_INT;
            (*fields)[field_idx].size = 4;
        } else if (strcmp(type_str, "string") == 0) {
            (*fields)[field_idx].type = TYPE_STRING;
            if (strlen(size_str) > 0) {
                (*fields)[field_idx].size = atoi(size_str);
            } else {
                (*fields)[field_idx].size = 64;  // default
            }
        } else {
            // Unknown type
            free(input_copy);
            free(*fields);
            return URAGE_INVALID_ARG;
        }
        
        field_idx++;
        name_token = strtok(NULL, " \t");
    }
    
    free(input_copy);
    
    // Calculate offsets
    uint32_t offset = 0;
    for (uint32_t i = 0; i < *field_count; i++) {
        (*fields)[i].offset = offset;
        offset += (*fields)[i].size;
    }
    
    return URAGE_OK;
}

// ==================== DATA SERIALIZATION ====================

static int parse_value(const char* value_str, uint8_t type, void* output) {
    if (type == TYPE_INT) {
        *((uint32_t*)output) = (uint32_t)atoi(value_str);
        return 1;
    } else if (type == TYPE_STRING) {
        // Remove quotes if present
        const char* start = value_str;
        char temp[256];
        strncpy(temp, value_str, 255);
        temp[255] = '\0';
        
        if (temp[0] == '"') {
            char* end = strrchr(temp, '"');
            if (end) *end = '\0';
            strcpy((char*)output, temp + 1);
        } else {
            strcpy((char*)output, temp);
        }
        return 1;
    }
    return 0;
}

urage_result_t type_serialize(FieldDef* fields, uint32_t field_count,
                              const char* field_values, void* buffer) {
    if (!fields || !buffer || !field_values) return URAGE_INVALID_ARG;
    
    char* values_copy = strdup(field_values);
    if (!values_copy) return URAGE_ERROR;
    
    char* token = strtok(values_copy, " \t");
    while (token) {
        // Parse field=value
        char* equals = strchr(token, '=');
        if (equals) {
            *equals = '\0';
            char* field_name = token;
            char* value_str = equals + 1;
            
            // Find matching field
            int found = 0;
            for (uint32_t i = 0; i < field_count; i++) {
                if (strcmp(fields[i].name, field_name) == 0) {
                    // Write value at field offset
                    void* dest = (char*)buffer + fields[i].offset;
                    parse_value(value_str, fields[i].type, dest);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                printf("Warning: Unknown field '%s' ignored\n", field_name);
            }
        }
        token = strtok(NULL, " \t");
    }
    
    free(values_copy);
    return URAGE_OK;
}

urage_result_t type_deserialize(FieldDef* fields, uint32_t field_count,
                                const void* buffer, char* output, size_t output_size) {
    if (!fields || !buffer || !output) return URAGE_INVALID_ARG;
    
    output[0] = '\0';
    size_t pos = 0;
    
    for (uint32_t i = 0; i < field_count; i++) {
        const void* src = (const char*)buffer + fields[i].offset;
        
        if (fields[i].type == TYPE_INT) {
            uint32_t value = *((uint32_t*)src);
            pos += snprintf(output + pos, output_size - pos, 
                           "%s=%u", fields[i].name, value);
        } else if (fields[i].type == TYPE_STRING) {
            char value[256];
            strncpy(value, (const char*)src, fields[i].size);
            value[fields[i].size - 1] = '\0';
            pos += snprintf(output + pos, output_size - pos, 
                           "%s=\"%s\"", fields[i].name, value);
        }
        
        if (i < field_count - 1 && pos < output_size - 1) {
            output[pos++] = ' ';
        }
    }
    
    return URAGE_OK;
}

// This would require implementing cursor functionality in your B-tree
// For now, here's a placeholder:

typedef struct {
    urage_db_t* db;
    void* internal_cursor;
    int valid;
} TypeIterator;

TypeIterator* type_iterator_create(urage_db_t* db) {
    if (!db) return NULL;
    
    TypeIterator* it = malloc(sizeof(TypeIterator));
    if (!it) return NULL;
    
    it->db = db;
    it->internal_cursor = NULL;
    it->valid = 0;
    
    // TODO: Initialize B-tree cursor
    // This would require adding cursor support to your btree.c
    
    return it;
}

int type_iterator_next(TypeIterator* it, char** name) {
    if (!it || !name) return 0;
    
    // TODO: Get next key with "type:" prefix
    // For now, return 0 (no more items)
    
    return 0;
}

void type_iterator_destroy(TypeIterator* it) {
    free(it);
}