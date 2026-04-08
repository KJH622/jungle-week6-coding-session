#include "schema.h"

#include "../infrastructure/file_io.h"
#include "../shared/text.h"

#include <ctype.h>
#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int extract_json_string_after_key(const char *json, const char *key, char *out,
                                         size_t out_sz, const char *start_at) {
  const char *p = start_at ? start_at : json;
  size_t klen = strlen(key);
  while ((p = strstr(p, key)) != NULL) {
    p += klen;
    while (*p && *p != ':') {
      p++;
    }
    if (*p != ':') {
      break;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) {
      p++;
    }
    if (*p != '"') {
      continue;
    }
    p++;
    size_t i = 0;
    while (*p && *p != '"') {
      if (i + 1 < out_sz) {
        out[i++] = *p;
      }
      p++;
    }
    if (*p != '"') {
      return 0;
    }
    out[i] = '\0';
    return 1;
  }
  return 0;
}

static int load_one_schema(const char *path, TableSchema *out) {
  char *json = read_entire_file(path);
  if (!json) {
    return 0;
  }

  char table_name[256];
  if (!extract_json_string_after_key(json, "\"table\"", table_name, sizeof(table_name), NULL)) {
    free(json);
    return 0;
  }

  out->name = xstrdup(table_name);
  out->column_count = 0;

  const char *p = json;
  while ((p = strstr(p, "\"name\"")) != NULL) {
    char col_name[256];
    if (!extract_json_string_after_key(json, "\"name\"", col_name, sizeof(col_name), p)) {
      free(json);
      return 0;
    }

    const char *probe = strstr(p, "\"type\"");
    if (!probe) {
      p += 6;
      continue;
    }

    char col_type[256];
    if (!extract_json_string_after_key(json, "\"type\"", col_type, sizeof(col_type), p)) {
      free(json);
      return 0;
    }

    if (out->column_count >= MAX_COLUMNS) {
      free(json);
      return 0;
    }
    out->columns[out->column_count].name = xstrdup(col_name);
    out->columns[out->column_count].type = xstrdup(col_type);
    out->column_count++;

    p = probe + 6;
  }

  free(json);
  return (out->column_count > 0);
}

int load_named_schema(const char *table_name, TableSchema *out) {
  char path[512];
  snprintf(path, sizeof(path), "%s.schema", table_name);
  memset(out, 0, sizeof(*out));
  return load_one_schema(path, out);
}

int load_required_schemas(SchemaRegistry *reg) {
  reg->table_count = 0;

  const char *required[] = {"users.schema", "products.schema"};
  for (int i = 0; i < 2; i++) {
    if (reg->table_count >= MAX_TABLES) {
      return 0;
    }
    if (!load_one_schema(required[i], &reg->tables[reg->table_count])) {
      return 0;
    }
    reg->table_count++;
  }
  return 1;
}

int list_schema_tables(StrVec *out) {
  DIR *dir = opendir(".");
  struct dirent *entry;

  strvec_init(out);
  if (!dir) {
    return 0;
  }

  while ((entry = readdir(dir)) != NULL) {
    size_t len = strlen(entry->d_name);
    if (len <= 7) {
      continue;
    }
    if (strcmp(entry->d_name + len - 7, ".schema") != 0) {
      continue;
    }

    char table_name[256];
    size_t copy_len = len - 7;
    if (copy_len >= sizeof(table_name)) {
      copy_len = sizeof(table_name) - 1;
    }
    memcpy(table_name, entry->d_name, copy_len);
    table_name[copy_len] = '\0';
    strvec_push(out, xstrdup(table_name));
  }

  closedir(dir);
  return 1;
}

void free_table_schema(TableSchema *schema) {
  if (!schema) {
    return;
  }

  free(schema->name);
  schema->name = NULL;

  for (int i = 0; i < schema->column_count; i++) {
    free(schema->columns[i].name);
    free(schema->columns[i].type);
    schema->columns[i].name = NULL;
    schema->columns[i].type = NULL;
  }
  schema->column_count = 0;
}

void free_schema_registry(SchemaRegistry *reg) {
  if (!reg) {
    return;
  }

  for (int i = 0; i < reg->table_count; i++) {
    free_table_schema(&reg->tables[i]);
  }
  reg->table_count = 0;
}

const TableSchema *find_table(const SchemaRegistry *reg, const char *name) {
  for (int i = 0; i < reg->table_count; i++) {
    if (ci_equal(reg->tables[i].name, name)) {
      return &reg->tables[i];
    }
  }
  return NULL;
}

int find_col_index(const TableSchema *schema, const char *col_name) {
  for (int i = 0; i < schema->column_count; i++) {
    if (ci_equal(schema->columns[i].name, col_name)) {
      return i;
    }
  }
  return -1;
}
