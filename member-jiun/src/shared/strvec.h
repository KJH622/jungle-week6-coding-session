#ifndef SHARED_STRVEC_H
#define SHARED_STRVEC_H

typedef struct {
  char **items;
  int count;
  int cap;
} StrVec;

void strvec_init(StrVec *v);
void strvec_push(StrVec *v, char *s);
void strvec_free(StrVec *v);

#endif
