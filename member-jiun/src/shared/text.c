#include "text.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *xstrdup(const char *s) {
  size_t n = strlen(s);
  char *p = (char *)malloc(n + 1);
  if (!p) {
    fprintf(stderr, "ERROR: file open failed\n");
    exit(1);
  }
  memcpy(p, s, n + 1);
  return p;
}

int ci_equal(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return 0;
    }
    a++;
    b++;
  }
  return (*a == '\0' && *b == '\0');
}

int ci_n_equal(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
      return 0;
    }
  }
  return 1;
}

const char *skip_ws(const char *p) {
  while (*p && isspace((unsigned char)*p)) {
    p++;
  }
  return p;
}

void rtrim_inplace(char *s) {
  int n = (int)strlen(s);
  while (n > 0 && isspace((unsigned char)s[n - 1])) {
    s[n - 1] = '\0';
    n--;
  }
}

void ltrim_ptr(const char **p) {
  while (**p && isspace((unsigned char)**p)) {
    (*p)++;
  }
}

int parse_identifier(const char **pp, char *out, size_t out_sz) {
  const char *p = *pp;
  p = skip_ws(p);
  if (!(isalpha((unsigned char)*p) || *p == '_')) {
    return 0;
  }

  size_t i = 0;
  while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
    if (i + 1 < out_sz) {
      out[i++] = *p;
    }
    p++;
  }
  out[i] = '\0';
  *pp = p;
  return 1;
}

int consume_keyword(const char **pp, const char *kw) {
  const char *p = skip_ws(*pp);
  size_t n = strlen(kw);
  if (!p[n]) {
    return 0;
  }
  if (!ci_n_equal(p, kw, n)) {
    return 0;
  }
  if (isalnum((unsigned char)p[n]) || p[n] == '_') {
    return 0;
  }
  *pp = p + n;
  return 1;
}

void trim_copy(const char *src, char *dst, size_t dst_sz) {
  const char *s = src;
  ltrim_ptr(&s);
  size_t n = strlen(s);
  while (n > 0 && isspace((unsigned char)s[n - 1])) {
    n--;
  }
  if (n + 1 > dst_sz) {
    n = dst_sz - 1;
  }
  memcpy(dst, s, n);
  dst[n] = '\0';
}

int is_integer_literal(const char *s) {
  if (!*s) {
    return 0;
  }
  if (*s == '+' || *s == '-') {
    s++;
  }
  if (!*s) {
    return 0;
  }
  while (*s) {
    if (!isdigit((unsigned char)*s)) {
      return 0;
    }
    s++;
  }
  return 1;
}
