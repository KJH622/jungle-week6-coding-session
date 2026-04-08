#include "interactive.h"

#include "../application/processor.h"
#include "../domain/parser.h"
#include "../domain/schema.h"
#include "../shared/strvec.h"
#include "../shared/text.h"
#include "../shared/ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

static void print_banner(void) {
  printf("%s", COLOR_CYAN);
  printf("                    ___                                                                                        \n");
  printf("                   /\\_ \\                                                                                       \n");
  printf("  ____     __      \\//\\ \\                _____    _ __    ___     ___      __     ____    ____    ___    _ __  \n");
  printf(" /',__\\  /'__`\\      \\ \\ \\              /\\ '__`\\\\/\\`'__\\ / __`\\  /'___\\  /'__`\\  /',__\\  /',__\\  / __`\\\\/\\`'__\\\\\n");
  printf("/\\__, `\\\\/\\ \\L\\ \\      \\_\\ \\_            \\ \\ \\L\\ \\\\ \\ \\/ /\\ \\L\\ \\/\\ \\__/ /\\  __/ /\\__, `\\\\/\\__, `\\\\/\\ \\L\\ \\\\ \\ \\/ \n");
  printf("\\/\\____/\\ \\___, \\     /\\____\\            \\ \\ ,__/ \\ \\_\\\\ \\____/\\ \\____\\\\ \\____\\\\/\\____/\\/\\____/\\ \\____/ \\ \\_\\ \n");
  printf(" \\/___/  \\/___/\\ \\    \\/____/             \\ \\ \\/   \\/_/ \\/___/  \\/____/ \\/____/ \\/___/  \\/___/  \\/___/   \\/_/ \n");
  printf("              \\ \\_\\                        \\ \\_\\                                                               \n");
  printf("               \\/_/                         \\/_/                                                               \n");
  printf("%s", COLOR_RESET);
  printf("%sSQL Processor v1.0%s\n", COLOR_BOLD, COLOR_RESET);
  printf("%sType .help for commands%s\n\n", COLOR_CYAN, COLOR_RESET);
}

static void print_help(void) {
  printf("%sAvailable commands%s\n", COLOR_BOLD, COLOR_RESET);
  printf("  .help              Show this help message\n");
  printf("  .quit              Exit interactive mode\n");
  printf("  .exit              Exit interactive mode\n");
  printf("  .tables            List available tables\n");
  printf("  .schema <table>    Show schema for a table\n");
  printf("\n");
  printf("%sSupported SQL%s\n", COLOR_BOLD, COLOR_RESET);
  printf("  INSERT INTO ... VALUES (...)\n");
  printf("  SELECT ... FROM ...\n");
}

static int compare_str_items(const void *a, const void *b) {
  const char *lhs = *(const char *const *)a;
  const char *rhs = *(const char *const *)b;
  return strcmp(lhs, rhs);
}

static void print_tables(void) {
  StrVec tables;
  if (!list_schema_tables(&tables)) {
    fprintf(stderr, "%sERROR: file open failed%s\n", COLOR_RED, COLOR_RESET);
    return;
  }

  if (tables.count > 1) {
    qsort(tables.items, (size_t)tables.count, sizeof(char *), compare_str_items);
  }

  printf("%sAvailable tables:%s\n", COLOR_BOLD, COLOR_RESET);
  for (int i = 0; i < tables.count; i++) {
    printf("  - %s\n", tables.items[i]);
  }
  if (tables.count == 0) {
    printf("  (none)\n");
  }

  strvec_free(&tables);
}

static void print_schema_for_table(const char *table_name) {
  TableSchema schema;
  if (!load_named_schema(table_name, &schema)) {
    fprintf(stderr, "%sERROR: table not found%s\n", COLOR_RED, COLOR_RESET);
    return;
  }

  printf("%sSchema for %s%s\n", COLOR_BOLD, table_name, COLOR_RESET);
  for (int i = 0; i < schema.column_count; i++) {
    printf("  - %s: %s\n", schema.columns[i].name, schema.columns[i].type);
  }
  free_table_schema(&schema);
}

static int handle_meta_command(char *line) {
  const char *cmd = line;
  ltrim_ptr(&cmd);

  if (strcmp(cmd, ".quit") == 0 || strcmp(cmd, ".exit") == 0) {
    printf("%sBye!%s\n", COLOR_GREEN, COLOR_RESET);
    return -1;
  }

  if (strcmp(cmd, ".help") == 0) {
    print_help();
    return 1;
  }

  if (strcmp(cmd, ".tables") == 0) {
    print_tables();
    return 1;
  }

  if (strncmp(cmd, ".schema", 7) == 0) {
    const char *arg = cmd + 7;
    arg = skip_ws(arg);
    if (*arg == '\0') {
      fprintf(stderr, "%sERROR: invalid query%s\n", COLOR_RED, COLOR_RESET);
      return 1;
    }

    char table_name[256];
    trim_copy(arg, table_name, sizeof(table_name));
    print_schema_for_table(table_name);
    return 1;
  }

  fprintf(stderr, "%sERROR: invalid query%s\n", COLOR_RED, COLOR_RESET);
  return 1;
}

static void print_statement_success(const StatementResult *result) {
  if (result->kind == STATEMENT_KIND_INSERT) {
    printf("%s✓ 1 row inserted.%s\n", COLOR_GREEN, COLOR_RESET);
    return;
  }

  if (result->kind == STATEMENT_KIND_SELECT) {
    printf("%s(%d row%s)%s\n", COLOR_GREEN, result->row_count,
           (result->row_count == 1) ? "" : "s", COLOR_RESET);
  }
}

static int run_sql_line(char *line) {
  StrVec statements;
  int had_error = 0;

  if (!split_statements(line, &statements)) {
    fprintf(stderr, "%sERROR: file open failed%s\n", COLOR_RED, COLOR_RESET);
    return 1;
  }

  if (statements.count == 0) {
    StatementResult result;
    if (!execute_sql_statement(line, 1, &result)) {
      strvec_free(&statements);
      fprintf(stderr, "%sERROR: %s%s\n", COLOR_RED,
              result.error_message ? result.error_message : "invalid query", COLOR_RESET);
      return 1;
    }

    strvec_free(&statements);
    print_statement_success(&result);
    return 0;
  }

  for (int i = 0; i < statements.count; i++) {
    StatementResult result;
    if (!execute_sql_statement(statements.items[i], 1, &result)) {
      fprintf(stderr, "%sERROR: %s%s\n", COLOR_RED,
              result.error_message ? result.error_message : "invalid query", COLOR_RESET);
      had_error = 1;
      continue;
    }
    print_statement_success(&result);
  }

  strvec_free(&statements);
  return had_error;
}

static char *read_prompt_line(void) {
#ifdef USE_READLINE
  const char *prompt = "\001" COLOR_GREEN "\002sql> \001" COLOR_RESET "\002";
  char *line = readline(prompt);
  if (line && line[0] != '\0') {
    add_history(line);
  }
  return line;
#else
  static char buffer[8192];
  fputs(COLOR_GREEN "sql> " COLOR_RESET, stdout);
  fflush(stdout);
  if (!fgets(buffer, sizeof(buffer), stdin)) {
    return NULL;
  }
  return xstrdup(buffer);
#endif
}

#ifdef USE_READLINE
static const char *const k_keywords[] = {"INSERT", "INTO", "SELECT", "FROM",
                                         "VALUES", "WHERE", NULL};
static const char *const k_commands[] = {".quit", ".exit", ".help", ".tables", ".schema", NULL};

static char *dup_if_match(const char *candidate, const char *text) {
  size_t text_len = strlen(text);
  if (strncmp(candidate, text, text_len) == 0) {
    return xstrdup(candidate);
  }
  return NULL;
}

static char *interactive_completion_generator(const char *text, int state) {
  static int keyword_index;
  static int command_index;
  static int table_index;
  static StrVec tables;

  if (state == 0) {
    keyword_index = 0;
    command_index = 0;
    table_index = 0;
    list_schema_tables(&tables);
  }

  while (k_commands[command_index]) {
    char *match = dup_if_match(k_commands[command_index++], text);
    if (match) {
      return match;
    }
  }

  while (k_keywords[keyword_index]) {
    char *match = dup_if_match(k_keywords[keyword_index++], text);
    if (match) {
      return match;
    }
  }

  while (table_index < tables.count) {
    char *match = dup_if_match(tables.items[table_index++], text);
    if (match) {
      return match;
    }
  }

  strvec_free(&tables);
  return NULL;
}

static char **interactive_completion(const char *text, int start, int end) {
  (void)start;
  (void)end;
  return rl_completion_matches(text, interactive_completion_generator);
}
#endif

int run_interactive_mode(void) {
  print_banner();

#ifdef USE_READLINE
  rl_attempted_completion_function = interactive_completion;
#endif

  while (1) {
    char *line = read_prompt_line();
    if (!line) {
      printf("%sBye!%s\n", COLOR_GREEN, COLOR_RESET);
      return 0;
    }

    rtrim_inplace(line);
    const char *trimmed = line;
    ltrim_ptr(&trimmed);

    if (*trimmed == '\0') {
      free(line);
      continue;
    }

    if (*trimmed == '.') {
      int meta_result = handle_meta_command(line);
      free(line);
      if (meta_result < 0) {
        return 0;
      }
      continue;
    }

    run_sql_line(line);
    free(line);
  }
}
