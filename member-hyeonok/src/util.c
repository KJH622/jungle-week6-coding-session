#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_entire_file(const char *path)
{
    FILE *file;
    long file_size;
    size_t read_size;
    char *buffer;

    /* SQL 입력 파일은 실행 시점의 현재 작업 디렉토리 기준으로 연다. */
    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)file_size + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    read_size = fread(buffer, 1, (size_t)file_size, file);
    if (read_size != (size_t)file_size) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[file_size] = '\0';
    fclose(file);
    return buffer;
}

char *trim_in_place(char *text)
{
    char *start;
    char *end;

    if (text == NULL) {
        return NULL;
    }

    start = text;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    if (*start == '\0') {
        *text = '\0';
        return text;
    }

    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }

    end[1] = '\0';
    return start;
}

char *duplicate_string(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

void free_string_array(char **items, size_t count)
{
    size_t index;

    if (items == NULL) {
        return;
    }

    for (index = 0; index < count; index++) {
        free(items[index]);
    }

    free(items);
}
