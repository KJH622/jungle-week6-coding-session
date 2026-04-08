#ifndef STORAGE_H
#define STORAGE_H

#include <string.h>

#include "types.h"

/* SELECT 결과를 채우기 전에 ResultSet 안을 전부 0으로 초기화합니다. */
static inline void result_set_reset(ResultSet *result) {
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
}

#endif
