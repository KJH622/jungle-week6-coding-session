#include "strvec.h"

#include <stdio.h>
#include <stdlib.h>

void strvec_init(StrVec *v) {
  v->items = NULL;
  v->count = 0;
  v->cap = 0;
}

void strvec_push(StrVec *v, char *s) {
  if (v->count == v->cap) {
    int new_cap = (v->cap == 0) ? 8 : v->cap * 2;
    char **next = (char **)realloc(v->items, sizeof(char *) * (size_t)new_cap);
    if (!next) {
      fprintf(stderr, "ERROR: file open failed\n");
      exit(1);
    }
    v->items = next;
    v->cap = new_cap;
  }
  v->items[v->count++] = s;
}

void strvec_free(StrVec *v) {
  for (int i = 0; i < v->count; i++) {
    free(v->items[i]);
  }
  free(v->items);
  v->items = NULL;
  v->count = 0;
  v->cap = 0;
}
