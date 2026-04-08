#ifndef APPLICATION_PROCESSOR_H
#define APPLICATION_PROCESSOR_H

typedef enum {
  STATEMENT_KIND_UNKNOWN = 0,
  STATEMENT_KIND_INSERT,
  STATEMENT_KIND_SELECT
} StatementKind;

typedef struct {
  int success;
  StatementKind kind;
  const char *error_message;
  int row_count;
} StatementResult;

int process_sql_file(const char *sql_file_path);
int execute_sql_text(const char *sql_text);
int execute_sql_statement(const char *stmt, int interactive_output, StatementResult *result);

#endif
