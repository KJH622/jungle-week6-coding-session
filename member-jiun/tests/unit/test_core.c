#include "../../src/domain/parser.h"
#include "../../src/domain/types.h"
#include "../../src/shared/strvec.h"
#include "../../src/shared/text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;

static void assert_true(int condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "[FAIL] %s\n", message);
    g_failures++;
  }
}

static void assert_str_eq(const char *actual, const char *expected, const char *message) {
  if (strcmp(actual, expected) != 0) {
    fprintf(stderr, "[FAIL] %s\n", message);
    fprintf(stderr, "  expected: %s\n", expected);
    fprintf(stderr, "  actual  : %s\n", actual);
    g_failures++;
  }
}

static SchemaRegistry make_registry(void) {
  SchemaRegistry reg;
  memset(&reg, 0, sizeof(reg));

  reg.table_count = 2;

  reg.tables[0].name = "users";
  reg.tables[0].column_count = 3;
  reg.tables[0].columns[0].name = "name";
  reg.tables[0].columns[0].type = "string";
  reg.tables[0].columns[1].name = "age";
  reg.tables[0].columns[1].type = "int";
  reg.tables[0].columns[2].name = "major";
  reg.tables[0].columns[2].type = "string";

  reg.tables[1].name = "products";
  reg.tables[1].column_count = 3;
  reg.tables[1].columns[0].name = "name";
  reg.tables[1].columns[0].type = "string";
  reg.tables[1].columns[1].name = "price";
  reg.tables[1].columns[1].type = "int";
  reg.tables[1].columns[2].name = "category";
  reg.tables[1].columns[2].type = "string";

  return reg;
}

static void test_text_helpers(void) {
  assert_true(ci_equal("SELECT", "select"), "ci_equal should ignore case");
  assert_true(ci_n_equal("InsertInto", "insert", 6), "ci_n_equal should compare prefixes");
  assert_true(is_integer_literal("25"), "integer literal should accept digits");
  assert_true(is_integer_literal("-12"), "integer literal should accept sign");
  assert_true(!is_integer_literal("2a"), "integer literal should reject invalid chars");
}

static void test_split_statements(void) {
  const char *sql =
      "INSERT INTO users (name, age, major) VALUES ('kim;min', 25, 'cs');;;\n"
      "SELECT name FROM users";
  StrVec statements;

  assert_true(split_statements(sql, &statements), "split_statements should succeed");
  assert_true(statements.count == 2, "split_statements should ignore empty statements");
  assert_str_eq(statements.items[0],
                "INSERT INTO users (name, age, major) VALUES ('kim;min', 25, 'cs')",
                "split_statements should keep semicolon inside quotes");
  assert_str_eq(statements.items[1], "SELECT name FROM users",
                "split_statements should keep trailing statement without semicolon");

  strvec_free(&statements);
}

static void test_parse_insert(void) {
  SchemaRegistry reg = make_registry();
  InsertPlan plan;
  const char *err_msg = NULL;

  assert_true(parse_insert(
                  "INSERT INTO users (name, age, major) VALUES ('Kim', 25, 'CS')", &reg, &plan,
                  &err_msg),
              "parse_insert should parse valid insert");
  assert_true(plan.schema == &reg.tables[0], "parse_insert should select users schema");
  assert_str_eq(plan.values[0], "Kim", "parse_insert should map string value");
  assert_str_eq(plan.values[1], "25", "parse_insert should map int value");
  assert_str_eq(plan.values[2], "CS", "parse_insert should map final value");

  err_msg = NULL;
  assert_true(!parse_insert(
                  "INSERT INTO users (name, age, major) VALUES ('Kim', 25)", &reg, &plan,
                  &err_msg),
              "parse_insert should reject mismatch counts");
  assert_str_eq(err_msg, "column count does not match value count",
                "parse_insert should report mismatch count error");

  err_msg = NULL;
  assert_true(!parse_insert(
                  "INSERT INTO orders (item, qty) VALUES ('Book', 1)", &reg, &plan, &err_msg),
              "parse_insert should reject unknown table");
  assert_str_eq(err_msg, "table not found", "parse_insert should report missing table");
}

static void test_parse_select(void) {
  SchemaRegistry reg = make_registry();
  SelectPlan plan;
  const char *err_msg = NULL;

  assert_true(parse_select("SELECT major, name FROM users", &reg, &plan, &err_msg),
              "parse_select should parse valid projection");
  assert_true(plan.selected_count == 2, "parse_select should keep requested column count");
  assert_true(plan.selected_idx[0] == 2, "parse_select should preserve column order");
  assert_true(plan.selected_idx[1] == 0, "parse_select should preserve second column order");

  err_msg = NULL;
  assert_true(!parse_select("SELECT email FROM users", &reg, &plan, &err_msg),
              "parse_select should reject unknown column");
  assert_str_eq(err_msg, "invalid query", "parse_select should report invalid query");

  err_msg = NULL;
  assert_true(!parse_select("SELECT * FROM orders", &reg, &plan, &err_msg),
              "parse_select should reject unknown table");
  assert_str_eq(err_msg, "table not found", "parse_select should report missing table");
}

int main(void) {
  test_text_helpers();
  test_split_statements();
  test_parse_insert();
  test_parse_select();

  if (g_failures != 0) {
    fprintf(stderr, "Total failures: %d\n", g_failures);
    return 1;
  }

  printf("All unit tests passed.\n");
  return 0;
}
