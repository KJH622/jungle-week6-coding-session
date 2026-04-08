#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>

char *util_strdup(const char *text);
char *util_strndup(const char *text, size_t length);
char *util_trim_copy_range(const char *start, const char *end);
void util_trim_inplace(char *text);
void util_lowercase_inplace(char *text);
int util_case_cmp(const char *left, const char *right);
bool util_case_equal(const char *left, const char *right);
bool util_case_starts_with(const char *text, const char *prefix);
bool util_is_blank_string(const char *text);
const char *util_skip_spaces(const char *text);
const char *util_find_keyword_outside_quotes(const char *text, const char *keyword);

#endif
