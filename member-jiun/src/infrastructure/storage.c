#include "storage.h"

#include "../shared/strvec.h"
#include "../shared/text.h"

#include <stdio.h>
#include <string.h>

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

int execute_insert(const InsertPlan *plan) {
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

int execute_select(const SelectPlan *plan) {
  char path[512];
  snprintf(path, sizeof(path), "%s.data", plan->schema->name);

  FILE *fp = fopen(path, "rb");
  if (!fp) {
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
