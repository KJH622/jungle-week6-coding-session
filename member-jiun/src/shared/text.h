#ifndef SHARED_TEXT_H
#define SHARED_TEXT_H

#include <stddef.h>

char *xstrdup(const char *s);
int ci_equal(const char *a, const char *b);
int ci_n_equal(const char *a, const char *b, size_t n);
const char *skip_ws(const char *p);
void rtrim_inplace(char *s);
void ltrim_ptr(const char **p);
int parse_identifier(const char **pp, char *out, size_t out_sz);
int consume_keyword(const char **pp, const char *kw);
void trim_copy(const char *src, char *dst, size_t dst_sz);
int is_integer_literal(const char *s);

#endif
