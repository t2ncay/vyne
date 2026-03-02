#ifndef TYPE_H
#define TYPE_H

#include <stdint.h>

#define TYPE_INT    1
#define TYPE_STRING 2

typedef struct {
    char name[32];
    uint32_t offset;
    uint8_t type;
    uint32_t size;  // for strings
} FieldDef;

typedef struct {
    uint32_t id;
    char name[64];
    uint32_t size;
    uint32_t field_count;
    FieldDef fields[];  // flexible array
} TypeDef;

#endif