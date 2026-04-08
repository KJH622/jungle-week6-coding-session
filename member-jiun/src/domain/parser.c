#include "parser.h"

#include "schema.h"
#include "../shared/text.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int split_csv_like_ident_list(const char *inside, char items[][256], int *count) {
  *count = 0;
  const char *p = inside;
  while (1) {
    p = skip_ws(p);
    if (!*p) {
      break;
    }

    char ident[256];
    if (!parse_identifier(&p, ident, sizeof(ident))) {
      return 0;
    }

    if (*count >= MAX_LIST_ITEMS) {
      return 0;
    }
    strncpy(items[*count], ident, 255);
    items[*count][255] = '\0';
    (*count)++;

    p = skip_ws(p);
    if (!*p) {
      break;
    }
    if (*p != ',') {
      return 0;
    }
    p++;
  }
  return 1;
}

static int split_values_list(const char *inside, char items[][4096], int *count,
                             int quoted_flags[]) {
  *count = 0;
  const char *p = inside;
  while (1) {
    p = skip_ws(p);
    if (!*p) {
      break;
    }

    if (*count >= MAX_LIST_ITEMS) {
      return 0;
    }

    if (*p == '\'') {
      quoted_flags[*count] = 1;
      p++;
      int closed = 0;
      size_t i = 0;
      while (*p) {
        if (*p == '\'') {
          if (*(p + 1) == '\'') {
            if (i + 1 < sizeof(items[*count])) {
              items[*count][i++] = '\'';
            }
            p += 2;
            continue;
          }
          p++;
          closed = 1;
          break;
        }
        if (i + 1 < sizeof(items[*count])) {
          items[*count][i++] = *p;
        }
        p++;
      }
      items[*count][i] = '\0';
      if (!closed) {
        return 0;
      }
    } else {
      quoted_flags[*count] = 0;
      char raw[4096];
      size_t i = 0;
      while (*p && *p != ',') {
        if (i + 1 < sizeof(raw)) {
          raw[i++] = *p;
        }
        p++;
      }
      raw[i] = '\0';
      trim_copy(raw, items[*count], sizeof(items[*count]));
      if (items[*count][0] == '\0') {
        return 0;
      }
    }

    (*count)++;

    p = skip_ws(p);
    if (!*p) {
      break;
    }
    if (*p != ',') {
      return 0;
    }
    p++;
  }
  return 1;
}

int parse_insert(const char *stmt, const SchemaRegistry *reg, InsertPlan *plan,
                 const char **err_msg) {
  const char *p = stmt;
  char table[256];

  if (!consume_keyword(&p, "INSERT") || !consume_keyword(&p, "INTO")) {
    *err_msg = "invalid query";
    return 0;
  }
  if (!parse_identifier(&p, table, sizeof(table))) {
    *err_msg = "invalid query";
    return 0;
  }

  const TableSchema *schema = find_table(reg, table);
  if (!schema) {
    *err_msg = "table not found";
    return 0;
  }

  p = skip_ws(p);
  if (*p != '(') {
    *err_msg = "invalid query";
    return 0;
  }
  p++;
  const char *cols_begin = p;
  int depth = 1;
  while (*p && depth > 0) {
    if (*p == ')') {
      depth--;
      if (depth == 0) {
        break;
      }
    }
    p++;
  }
  if (*p != ')') {
    *err_msg = "invalid query";
    return 0;
  }

  size_t cols_len = (size_t)(p - cols_begin);
  char *cols = (char *)malloc(cols_len + 1);
  if (!cols) {
    *err_msg = "file open failed";
    return 0;
  }
  memcpy(cols, cols_begin, cols_len);
  cols[cols_len] = '\0';
  p++;

  if (!consume_keyword(&p, "VALUES")) {
    free(cols);
    *err_msg = "invalid query";
    return 0;
  }

  p = skip_ws(p);
  if (*p != '(') {
    free(cols);
    *err_msg = "invalid query";
    return 0;
  }
  p++;
  const char *vals_begin = p;
  int in_quote = 0;
  while (*p) {
    if (*p == '\'' && *(p + 1) == '\'' && in_quote) {
      p += 2;
      continue;
    }
    if (*p == '\'') {
      in_quote = !in_quote;
      p++;
      continue;
    }
    if (*p == ')' && !in_quote) {
      break;
    }
    p++;
  }
  if (*p != ')') {
    free(cols);
    *err_msg = "invalid query";
    return 0;
  }

  size_t vals_len = (size_t)(p - vals_begin);
  char *vals = (char *)malloc(vals_len + 1);
  if (!vals) {
    free(cols);
    *err_msg = "file open failed";
    return 0;
  }
  memcpy(vals, vals_begin, vals_len);
  vals[vals_len] = '\0';
  p++;

  p = skip_ws(p);
  if (*p != '\0') {
    free(cols);
    free(vals);
    *err_msg = "invalid query";
    return 0;
  }

  char col_items[MAX_LIST_ITEMS][256];
  int col_count = 0;
  if (!split_csv_like_ident_list(cols, col_items, &col_count)) {
    free(cols);
    free(vals);
    *err_msg = "invalid query";
    return 0;
  }

  char val_items[MAX_LIST_ITEMS][4096];
  int quoted_flags[MAX_LIST_ITEMS] = {0};
  int val_count = 0;
  if (!split_values_list(vals, val_items, &val_count, quoted_flags)) {
    free(cols);
    free(vals);
    *err_msg = "invalid query";
    return 0;
  }

  free(cols);
  free(vals);

  if (col_count != val_count || col_count != schema->column_count) {
    *err_msg = "column count does not match value count";
    return 0;
  }

  int assigned[MAX_COLUMNS] = {0};
  for (int i = 0; i < schema->column_count; i++) {
    plan->values[i][0] = '\0';
  }

  for (int i = 0; i < col_count; i++) {
    int idx = find_col_index(schema, col_items[i]);
    if (idx < 0 || assigned[idx]) {
      *err_msg = "invalid query";
      return 0;
    }

    if (ci_equal(schema->columns[idx].type, "int")) {
      if (quoted_flags[i] || !is_integer_literal(val_items[i])) {
        *err_msg = "invalid query";
        return 0;
      }
    }

    strncpy(plan->values[idx], val_items[i], sizeof(plan->values[idx]) - 1);
    plan->values[idx][sizeof(plan->values[idx]) - 1] = '\0';
    assigned[idx] = 1;
  }

  for (int i = 0; i < schema->column_count; i++) {
    if (!assigned[i]) {
      *err_msg = "column count does not match value count";
      return 0;
    }
  }

  plan->schema = schema;
  return 1;
}

int parse_select(const char *stmt, const SchemaRegistry *reg, SelectPlan *plan,
                 const char **err_msg) {
  const char *p = stmt;
  if (!consume_keyword(&p, "SELECT")) {
    *err_msg = "invalid query";
    return 0;
  }

  p = skip_ws(p);

  int select_all = 0;
  char col_items[MAX_LIST_ITEMS][256];
  int col_count = 0;

  if (*p == '*') {
    select_all = 1;
    p++;
  } else {
    const char *cols_begin = p;
    while (*p) {
      if ((tolower((unsigned char)p[0]) == 'f') && (tolower((unsigned char)p[1]) == 'r') &&
          (tolower((unsigned char)p[2]) == 'o') && (tolower((unsigned char)p[3]) == 'm')) {
        const char *q = p + 4;
        if (*q == '\0' || isspace((unsigned char)*q)) {
          break;
        }
      }
      p++;
    }
    if (*p == '\0') {
      *err_msg = "invalid query";
      return 0;
    }

    size_t len = (size_t)(p - cols_begin);
    char *cols = (char *)malloc(len + 1);
    if (!cols) {
      *err_msg = "file open failed";
      return 0;
    }
    memcpy(cols, cols_begin, len);
    cols[len] = '\0';

    if (!split_csv_like_ident_list(cols, col_items, &col_count)) {
      free(cols);
      *err_msg = "invalid query";
      return 0;
    }
    free(cols);
  }

  if (!consume_keyword(&p, "FROM")) {
    *err_msg = "invalid query";
    return 0;
  }

  char table[256];
  if (!parse_identifier(&p, table, sizeof(table))) {
    *err_msg = "invalid query";
    return 0;
  }

  p = skip_ws(p);
  if (*p != '\0') {
    *err_msg = "invalid query";
    return 0;
  }

  const TableSchema *schema = find_table(reg, table);
  if (!schema) {
    *err_msg = "table not found";
    return 0;
  }

  plan->schema = schema;
  if (select_all) {
    plan->selected_count = schema->column_count;
    for (int i = 0; i < schema->column_count; i++) {
      plan->selected_idx[i] = i;
    }
  } else {
    plan->selected_count = col_count;
    for (int i = 0; i < col_count; i++) {
      int idx = find_col_index(schema, col_items[i]);
      if (idx < 0) {
        *err_msg = "invalid query";
        return 0;
      }
      plan->selected_idx[i] = idx;
    }
  }
  return 1;
}

int split_statements(const char *sql, StrVec *out) {
  strvec_init(out);

  size_t n = strlen(sql);
  size_t start = 0;
  int in_quote = 0;

  for (size_t i = 0; i < n; i++) {
    char c = sql[i];
    if (c == '\'' && in_quote && (i + 1 < n) && sql[i + 1] == '\'') {
      i++;
      continue;
    }
    if (c == '\'') {
      in_quote = !in_quote;
      continue;
    }
    if (c == ';' && !in_quote) {
      size_t len = i - start;
      char *stmt = (char *)malloc(len + 1);
      if (!stmt) {
        return 0;
      }
      memcpy(stmt, sql + start, len);
      stmt[len] = '\0';

      const char *p = stmt;
      ltrim_ptr(&p);
      if (*p) {
        char *trimmed = xstrdup(p);
        rtrim_inplace(trimmed);
        if (trimmed[0] != '\0') {
          strvec_push(out, trimmed);
        } else {
          free(trimmed);
        }
      }
      free(stmt);
      start = i + 1;
    }
  }

  if (start < n) {
    size_t len = n - start;
    char *stmt = (char *)malloc(len + 1);
    if (!stmt) {
      return 0;
    }
    memcpy(stmt, sql + start, len);
    stmt[len] = '\0';
    const char *p = stmt;
    ltrim_ptr(&p);
    if (*p) {
      char *trimmed = xstrdup(p);
      rtrim_inplace(trimmed);
      if (trimmed[0] != '\0') {
        strvec_push(out, trimmed);
      } else {
        free(trimmed);
      }
    }
    free(stmt);
  }

  return 1;
}

int is_insert_stmt(const char *stmt) {
  const char *p = skip_ws(stmt);
  return ci_n_equal(p, "INSERT", 6);
}

int is_select_stmt(const char *stmt) {
  const char *p = skip_ws(stmt);
  return ci_n_equal(p, "SELECT", 6);
}
