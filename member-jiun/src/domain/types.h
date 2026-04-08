#ifndef DOMAIN_TYPES_H
#define DOMAIN_TYPES_H

#define MAX_TABLES 16
#define MAX_COLUMNS 64
#define MAX_LIST_ITEMS 64

typedef struct {
  char *name;
  char *type;
} Column;

typedef struct {
  char *name;
  int column_count;
  Column columns[MAX_COLUMNS];
} TableSchema;

typedef struct {
  int table_count;
  TableSchema tables[MAX_TABLES];
} SchemaRegistry;

typedef struct {
  const TableSchema *schema;
  char values[MAX_COLUMNS][4096];
} InsertPlan;

typedef struct {
  const TableSchema *schema;
  int selected_idx[MAX_COLUMNS];
  int selected_count;
} SelectPlan;

#endif
