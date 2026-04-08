#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
  char **items;
  int count;
  int cap;
} StrVec;

typedef struct {
  const TableSchema *schema;
  char values[MAX_COLUMNS][4096];
} InsertPlan;

typedef struct {
  const TableSchema *schema;
  int selected_idx[MAX_COLUMNS];
  int selected_count;
} SelectPlan;

static void strvec_init(StrVec *v) {
  v->items = NULL;
  v->count = 0;
  v->cap = 0;
}

static void strvec_push(StrVec *v, char *s) {
  if (v->count == v->cap) {
    int new_cap = (v->cap == 0) ? 8 : v->cap * 2;
    char **next = (char **)realloc(v->items, sizeof(char *) * (size_t)new_cap);
    if (!next) {
      fprintf(stderr, "ERROR: file open failed\n");
      exit(1);
    }
    v->items = next;
    v->cap = new_cap;
  }
  v->items[v->count++] = s;
}

static void strvec_free(StrVec *v) {
  for (int i = 0; i < v->count; i++) {
    free(v->items[i]);
  }
  free(v->items);
  v->items = NULL;
  v->count = 0;
  v->cap = 0;
}

static void print_error(const char *msg) { fprintf(stderr, "ERROR: %s\n", msg); }

static char *xstrdup(const char *s) {
  size_t n = strlen(s);
  char *p = (char *)malloc(n + 1);
  if (!p) {
    fprintf(stderr, "ERROR: file open failed\n");
    exit(1);
  }
  memcpy(p, s, n + 1);
  return p;
}

static int ci_equal(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return 0;
    }
    a++;
    b++;
  }
  return (*a == '\0' && *b == '\0');
}

static int ci_n_equal(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
      return 0;
    }
  }
  return 1;
}

static const char *skip_ws(const char *p) {
  while (*p && isspace((unsigned char)*p)) {
    p++;
  }
  return p;
}

static void rtrim_inplace(char *s) {
  int n = (int)strlen(s);
  while (n > 0 && isspace((unsigned char)s[n - 1])) {
    s[n - 1] = '\0';
    n--;
  }
}

static void ltrim_ptr(const char **p) {
  while (**p && isspace((unsigned char)**p)) {
    (*p)++;
  }
}

static int parse_identifier(const char **pp, char *out, size_t out_sz) {
  const char *p = *pp;
  p = skip_ws(p);
  if (!(isalpha((unsigned char)*p) || *p == '_')) {
    return 0;
  }
  size_t i = 0;
  while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
    if (i + 1 < out_sz) {
      out[i++] = *p;
    }
    p++;
  }
  out[i] = '\0';
  *pp = p;
  return 1;
}

static int consume_keyword(const char **pp, const char *kw) {
  const char *p = skip_ws(*pp);
  size_t n = strlen(kw);
  if (!p[n]) {
    return 0;
  }
  if (!ci_n_equal(p, kw, n)) {
    return 0;
  }
  if (isalnum((unsigned char)p[n]) || p[n] == '_') {
    return 0;
  }
  *pp = p + n;
  return 1;
}

static char *read_entire_file(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
    return NULL;
  }

  FILE *fp = fopen(path, "rb");
  if (!fp) {
    return NULL;
  }
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return NULL;
  }
  long sz = ftell(fp);
  if (sz < 0) {
    fclose(fp);
    return NULL;
  }
  if (fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return NULL;
  }
  char *buf = (char *)malloc((size_t)sz + 1);
  if (!buf) {
    fclose(fp);
    return NULL;
  }
  size_t n = fread(buf, 1, (size_t)sz, fp);
  fclose(fp);
  buf[n] = '\0';
  return buf;
}

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
    /* Skip accidental capture of table-level key by requiring proximity of "type" key. */
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

static int load_required_schemas(SchemaRegistry *reg) {
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

static const TableSchema *find_table(const SchemaRegistry *reg, const char *name) {
  for (int i = 0; i < reg->table_count; i++) {
    if (ci_equal(reg->tables[i].name, name)) {
      return &reg->tables[i];
    }
  }
  return NULL;
}

static int find_col_index(const TableSchema *schema, const char *col_name) {
  for (int i = 0; i < schema->column_count; i++) {
    if (ci_equal(schema->columns[i].name, col_name)) {
      return i;
    }
  }
  return -1;
}

static void trim_copy(const char *src, char *dst, size_t dst_sz) {
  const char *s = src;
  ltrim_ptr(&s);
  size_t n = strlen(s);
  while (n > 0 && isspace((unsigned char)s[n - 1])) {
    n--;
  }
  if (n + 1 > dst_sz) {
    n = dst_sz - 1;
  }
  memcpy(dst, s, n);
  dst[n] = '\0';
}

static int split_csv_like_ident_list(const char *inside, char items[][256], int *count) {
  *count = 0;
  const char *p = inside;
  while (1) {
    p = skip_ws(p);
    if (!*p) {
      break;
    }

    char ident[256];
    const char *before = p;
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
      (void)before;
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

static int is_integer_literal(const char *s) {
  if (!*s) {
    return 0;
  }
  if (*s == '+' || *s == '-') {
    s++;
  }
  if (!*s) {
    return 0;
  }
  while (*s) {
    if (!isdigit((unsigned char)*s)) {
      return 0;
    }
    s++;
  }
  return 1;
}

static int parse_insert(const char *stmt, const SchemaRegistry *reg, InsertPlan *plan,
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
    if (idx < 0) {
      *err_msg = "invalid query";
      return 0;
    }
    if (assigned[idx]) {
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

static int parse_select(const char *stmt, const SchemaRegistry *reg, SelectPlan *plan,
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

static void encode_cell(const char *src, char *dst, size_t dst_sz) {
  size_t j = 0;
  for (size_t i = 0; src[i] && j + 2 < dst_sz; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c == '\\') {
      dst[j++] = '\\';
      dst[j++] = '\\';
    } else if (c == '\t') {
      dst[j++] = '\\';
      dst[j++] = 't';
    } else if (c == '\n') {
      dst[j++] = '\\';
      dst[j++] = 'n';
    } else if (c == '\r') {
      dst[j++] = '\\';
      dst[j++] = 'r';
    } else {
      dst[j++] = (char)c;
    }
  }
  dst[j] = '\0';
}

static void decode_cell(const char *src, char *dst, size_t dst_sz) {
  size_t j = 0;
  for (size_t i = 0; src[i] && j + 1 < dst_sz; i++) {
    if (src[i] == '\\' && src[i + 1]) {
      i++;
      if (src[i] == 'n') {
        dst[j++] = '\n';
      } else if (src[i] == 't') {
        dst[j++] = '\t';
      } else if (src[i] == 'r') {
        dst[j++] = '\r';
      } else {
        dst[j++] = src[i];
      }
    } else {
      dst[j++] = src[i];
    }
  }
  dst[j] = '\0';
}

static int execute_insert(const InsertPlan *plan) {
  char path[512];
  snprintf(path, sizeof(path), "%s.data", plan->schema->name);

  FILE *fp = fopen(path, "ab");
  if (!fp) {
    return 0;
  }

  for (int i = 0; i < plan->schema->column_count; i++) {
    char enc[8192];
    encode_cell(plan->values[i], enc, sizeof(enc));
    fputs(enc, fp);
    if (i + 1 < plan->schema->column_count) {
      fputc('\t', fp);
    }
  }
  fputc('\n', fp);
  fclose(fp);
  return 1;
}

static int split_tab_line(char *line, char fields[][4096], int max_fields) {
  int count = 0;
  char *saveptr = NULL;
  char *tok = strtok_r(line, "\t", &saveptr);
  while (tok && count < max_fields) {
    decode_cell(tok, fields[count], sizeof(fields[count]));
    count++;
    tok = strtok_r(NULL, "\t", &saveptr);
  }
  return count;
}

static int execute_select(const SelectPlan *plan) {
  char path[512];
  snprintf(path, sizeof(path), "%s.data", plan->schema->name);

  FILE *fp = fopen(path, "rb");
  if (!fp) {
    /* No data file means empty table. */
    return 1;
  }

  StrVec rows;
  strvec_init(&rows);

  char line[32768];
  while (fgets(line, sizeof(line), fp) != NULL) {
    rtrim_inplace(line);
    if (line[0] == '\0') {
      continue;
    }
    strvec_push(&rows, xstrdup(line));
  }
  fclose(fp);

  if (rows.count == 0) {
    strvec_free(&rows);
    return 1;
  }

  for (int i = 0; i < plan->selected_count; i++) {
    const char *name = plan->schema->columns[plan->selected_idx[i]].name;
    fputs(name, stdout);
    if (i + 1 < plan->selected_count) {
      fputc(',', stdout);
    }
  }
  fputc('\n', stdout);

  for (int r = 0; r < rows.count; r++) {
    char tmp[32768];
    strncpy(tmp, rows.items[r], sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char fields[MAX_COLUMNS][4096];
    int got = split_tab_line(tmp, fields, MAX_COLUMNS);
    if (got < plan->schema->column_count) {
      continue;
    }

    for (int i = 0; i < plan->selected_count; i++) {
      int idx = plan->selected_idx[i];
      fputs(fields[idx], stdout);
      if (i + 1 < plan->selected_count) {
        fputc(',', stdout);
      }
    }
    fputc('\n', stdout);
  }

  strvec_free(&rows);
  return 1;
}

static int split_statements(const char *sql, StrVec *out) {
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

static int is_insert_stmt(const char *stmt) {
  const char *p = skip_ws(stmt);
  return ci_n_equal(p, "INSERT", 6);
}

static int is_select_stmt(const char *stmt) {
  const char *p = skip_ws(stmt);
  return ci_n_equal(p, "SELECT", 6);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    print_error("file open failed");
    return 1;
  }

  char *sql_text = read_entire_file(argv[1]);
  if (!sql_text) {
    print_error("file open failed");
    return 1;
  }

  StrVec statements;
  if (!split_statements(sql_text, &statements)) {
    free(sql_text);
    print_error("file open failed");
    return 1;
  }
  free(sql_text);

  if (statements.count == 0) {
    strvec_free(&statements);
    return 0;
  }

  SchemaRegistry reg;
  if (!load_required_schemas(&reg)) {
    strvec_free(&statements);
    print_error("file open failed");
    return 1;
  }

  int had_error = 0;

  for (int i = 0; i < statements.count; i++) {
    const char *stmt = statements.items[i];
    const char *err_msg = NULL;

    if (is_insert_stmt(stmt)) {
      InsertPlan plan;
      if (!parse_insert(stmt, &reg, &plan, &err_msg)) {
        print_error(err_msg ? err_msg : "invalid query");
        had_error = 1;
        continue;
      }
      if (!execute_insert(&plan)) {
        print_error("file open failed");
        had_error = 1;
        continue;
      }
      continue;
    }

    if (is_select_stmt(stmt)) {
      SelectPlan plan;
      if (!parse_select(stmt, &reg, &plan, &err_msg)) {
        print_error(err_msg ? err_msg : "invalid query");
        had_error = 1;
        continue;
      }
      if (!execute_select(&plan)) {
        print_error("file open failed");
        had_error = 1;
        continue;
      }
      continue;
    }

    print_error("invalid query");
    had_error = 1;
  }

  strvec_free(&statements);
  return had_error ? 1 : 0;
}
