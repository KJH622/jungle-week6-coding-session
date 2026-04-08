#ifndef AST_H
#define AST_H

typedef enum SqlStatementType {
    SQL_STATEMENT_UNCLASSIFIED = 0
} SqlStatementType;

typedef struct SqlStatement {
    SqlStatementType type;
    char *raw_sql;
} SqlStatement;

#endif
