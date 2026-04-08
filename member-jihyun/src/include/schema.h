#ifndef SCHEMA_H
#define SCHEMA_H

#include <stddef.h>

#include "status.h"

typedef struct SchemaColumn {
    char *name;
    char *type;
} SchemaColumn;

typedef struct TableSchema {
    char *table_name;
    size_t column_count;
    SchemaColumn *columns;
} TableSchema;

SqlStatus schema_load(const char *schema_dir, const char *table_name, TableSchema *out_schema);
int schema_find_column(const TableSchema *schema, const char *column_name);
void schema_destroy(TableSchema *schema);

#endif
