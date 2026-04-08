#include "include/utils.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* 문자열 전체를 새 메모리에 복사해 반환한다. */
char *util_strdup(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1U);
    return copy;
}

/* 지정 길이만큼 문자열을 복사해 널 종료 문자열로 만든다. */
char *util_strndup(const char *text, size_t length)
{
    char *copy;

    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

/* 문자열의 앞뒤 공백을 제자리에서 제거한다. */
void util_trim_inplace(char *text)
{
    char *start;
    char *end;
    size_t trimmed_length;

    if (text == NULL || text[0] == '\0') {
        return;
    }

    start = text;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }

    trimmed_length = (size_t)(end - start);
    if (start != text && trimmed_length > 0U) {
        memmove(text, start, trimmed_length);
    } else if (trimmed_length == 0U) {
        text[0] = '\0';
        return;
    }

    text[trimmed_length] = '\0';
}

/* 범위 문자열을 잘라내며 앞뒤 공백을 제거한 새 문자열을 반환한다. */
char *util_trim_copy_range(const char *start, const char *end)
{
    const char *left;
    const char *right;

    if (start == NULL || end == NULL || end < start) {
        return util_strdup("");
    }

    left = start;
    right = end;

    while (left < right && isspace((unsigned char)*left)) {
        left++;
    }

    while (right > left && isspace((unsigned char)*(right - 1))) {
        right--;
    }

    return util_strndup(left, (size_t)(right - left));
}

/* 문자열을 제자리에서 소문자로 변환한다. */
void util_lowercase_inplace(char *text)
{
    size_t index;

    if (text == NULL) {
        return;
    }

    for (index = 0U; text[index] != '\0'; index++) {
        text[index] = (char)tolower((unsigned char)text[index]);
    }
}

/* 대소문자를 무시하고 두 문자열을 비교한다. */
int util_case_cmp(const char *left, const char *right)
{
    unsigned char left_char;
    unsigned char right_char;

    if (left == NULL && right == NULL) {
        return 0;
    }
    if (left == NULL) {
        return -1;
    }
    if (right == NULL) {
        return 1;
    }

    while (*left != '\0' && *right != '\0') {
        left_char = (unsigned char)tolower((unsigned char)*left);
        right_char = (unsigned char)tolower((unsigned char)*right);
        if (left_char != right_char) {
            return (int)left_char - (int)right_char;
        }
        left++;
        right++;
    }

    return (int)(unsigned char)tolower((unsigned char)*left) -
           (int)(unsigned char)tolower((unsigned char)*right);
}

/* 두 문자열이 대소문자 무시 기준으로 같은지 반환한다. */
bool util_case_equal(const char *left, const char *right)
{
    return util_case_cmp(left, right) == 0;
}

/* 문자열이 prefix로 시작하는지(대소문자 무시) 검사한다. */
bool util_case_starts_with(const char *text, const char *prefix)
{
    size_t index;

    if (text == NULL || prefix == NULL) {
        return false;
    }

    for (index = 0U; prefix[index] != '\0'; index++) {
        if (text[index] == '\0') {
            return false;
        }
        if (tolower((unsigned char)text[index]) != tolower((unsigned char)prefix[index])) {
            return false;
        }
    }

    return true;
}

/* 문자열이 비어 있거나 공백 문자만 포함하는지 확인한다. */
bool util_is_blank_string(const char *text)
{
    if (text == NULL) {
        return true;
    }

    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return false;
        }
        text++;
    }

    return true;
}

/* 현재 포인터에서 연속된 공백을 건너뛴 위치를 반환한다. */
const char *util_skip_spaces(const char *text)
{
    while (text != NULL && *text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    return text;
}

/* 키워드 경계를 판별하기 위한 식별자 종료 문자 여부를 확인한다. */
static bool is_identifier_boundary(char value)
{
    return value == '\0' || isspace((unsigned char)value) || value == ',' || value == '(' || value == ')' || value == ';';
}

/* 따옴표 바깥에서만 키워드를 찾아 그 시작 위치를 반환한다. */
const char *util_find_keyword_outside_quotes(const char *text, const char *keyword)
{
    size_t keyword_length;
    bool in_quotes;
    size_t index;

    if (text == NULL || keyword == NULL) {
        return NULL;
    }

    keyword_length = strlen(keyword);
    in_quotes = false;

    for (index = 0U; text[index] != '\0'; index++) {
        if (text[index] == '\'') {
            if (in_quotes && text[index + 1U] == '\'') {
                index++;
                continue;
            }
            in_quotes = !in_quotes;
            continue;
        }

        if (!in_quotes && index > 0U && !is_identifier_boundary(text[index - 1U])) {
            continue;
        }

        if (!in_quotes && util_case_starts_with(text + index, keyword)) {
            if (is_identifier_boundary(text[index + keyword_length])) {
                return text + index;
            }
        }
    }

    return NULL;
}
