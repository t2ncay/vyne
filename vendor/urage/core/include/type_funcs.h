#ifndef TYPE_FUNCS_H
#define TYPE_FUNCS_H

#include "urage.h"
#include "type.h"

// Type management functions
urage_result_t type_create(urage_db_t* db, const char* name, 
                           FieldDef* fields, uint32_t field_count);
TypeDef* type_get(urage_db_t* db, const char* name);
urage_result_t type_delete(urage_db_t* db, const char* name);

// Serialization functions
urage_result_t type_serialize(FieldDef* fields, uint32_t field_count,
                              const char* field_values, void* buffer);
urage_result_t type_deserialize(FieldDef* fields, uint32_t field_count,
                                const void* buffer, char* output, size_t output_size);

#endif