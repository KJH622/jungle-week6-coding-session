#ifndef INFRASTRUCTURE_STORAGE_H
#define INFRASTRUCTURE_STORAGE_H

#include "../domain/types.h"

int execute_insert(const InsertPlan *plan);
int execute_select(const SelectPlan *plan);

#endif
