#include "processor.h"

#include "../domain/parser.h"
#include "../domain/schema.h"
#include "../infrastructure/file_io.h"
#include "../infrastructure/storage.h"
#include "../shared/error.h"

#include <stdlib.h>
#include <string.h>

static int execute_sql_text_with_mode(const char *sql_text, int interactive_output) {
  StrVec statements;
  if (!split_statements(sql_text, &statements)) {
    print_error("file open failed");
    return 1;
  }

  if (statements.count == 0) {
    strvec_free(&statements);
    return 0;
  }

  int had_error = 0;
  for (int i = 0; i < statements.count; i++) {
    StatementResult result;
    if (!execute_sql_statement(statements.items[i], interactive_output, &result)) {
      print_error(result.error_message ? result.error_message : "invalid query");
      had_error = 1;
    }
  }

  strvec_free(&statements);
  return had_error ? 1 : 0;
}

int process_sql_file(const char *sql_file_path) {
  char *sql_text = read_entire_file(sql_file_path);
  if (!sql_text) {
    print_error("file open failed");
    return 1;
  }

  int exit_code = execute_sql_text(sql_text);
  free(sql_text);
  return exit_code;
}

int execute_sql_text(const char *sql_text) { return execute_sql_text_with_mode(sql_text, 0); }

int execute_sql_statement(const char *stmt, int interactive_output, StatementResult *result) {
  SchemaRegistry reg;
  const char *err_msg = NULL;

  if (result) {
    memset(result, 0, sizeof(*result));
  }

  if (!load_required_schemas(&reg)) {
    if (result) {
      result->success = 0;
      result->error_message = "file open failed";
    }
    return 0;
  }

  if (is_insert_stmt(stmt)) {
    InsertPlan plan;
    if (!parse_insert(stmt, &reg, &plan, &err_msg)) {
      free_schema_registry(&reg);
      if (result) {
        result->success = 0;
        result->kind = STATEMENT_KIND_INSERT;
        result->error_message = err_msg ? err_msg : "invalid query";
      }
      return 0;
    }

    if (!execute_insert(&plan)) {
      free_schema_registry(&reg);
      if (result) {
        result->success = 0;
        result->kind = STATEMENT_KIND_INSERT;
        result->error_message = "file open failed";
      }
      return 0;
    }

    free_schema_registry(&reg);
    if (result) {
      result->success = 1;
      result->kind = STATEMENT_KIND_INSERT;
      result->row_count = 1;
    }
    return 1;
  }

  if (is_select_stmt(stmt)) {
    SelectPlan plan;
    if (!parse_select(stmt, &reg, &plan, &err_msg)) {
      free_schema_registry(&reg);
      if (result) {
        result->success = 0;
        result->kind = STATEMENT_KIND_SELECT;
        result->error_message = err_msg ? err_msg : "invalid query";
      }
      return 0;
    }

    SelectOutputOptions options;
    SelectOutputOptions *options_ptr = NULL;
    int row_count = 0;
    if (interactive_output) {
      options.interactive = 1;
      options_ptr = &options;
    }

    if (!execute_select_with_options(&plan, options_ptr, &row_count)) {
      free_schema_registry(&reg);
      if (result) {
        result->success = 0;
        result->kind = STATEMENT_KIND_SELECT;
        result->error_message = "file open failed";
      }
      return 0;
    }

    free_schema_registry(&reg);
    if (result) {
      result->success = 1;
      result->kind = STATEMENT_KIND_SELECT;
      result->row_count = row_count;
    }
    return 1;
  }

  free_schema_registry(&reg);
  if (result) {
    result->success = 0;
    result->kind = STATEMENT_KIND_UNKNOWN;
    result->error_message = "invalid query";
  }
  return 0;
}
