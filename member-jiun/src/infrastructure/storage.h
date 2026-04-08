#ifndef INFRASTRUCTURE_STORAGE_H
#define INFRASTRUCTURE_STORAGE_H

#include "../domain/types.h"

typedef struct {
  int interactive;
} SelectOutputOptions;

int execute_insert(const InsertPlan *plan);
int execute_select(const SelectPlan *plan);
int execute_select_with_options(const SelectPlan *plan, const SelectOutputOptions *options,
                                int *row_count);

#endif
