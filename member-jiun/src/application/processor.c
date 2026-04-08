#include "processor.h"

#include "../domain/parser.h"
#include "../domain/schema.h"
#include "../infrastructure/file_io.h"
#include "../infrastructure/storage.h"
#include "../shared/error.h"

#include <stdlib.h>

int process_sql_file(const char *sql_file_path) {
  char *sql_text = read_entire_file(sql_file_path);
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
