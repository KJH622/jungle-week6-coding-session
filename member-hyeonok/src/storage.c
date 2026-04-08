#include "storage.h"

#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int build_table_data_path(const char *table_name, char *path_buffer, size_t path_size)
{
    if (table_name == NULL || path_buffer == NULL || path_size == 0) {
        return 0;
    }

    return snprintf(path_buffer, path_size, "%s.data", table_name) < (int)path_size;
}

static int write_escaped_text(FILE *file, const char *text)
{
    const char *cursor;

    /*
     * 데이터 파일은 탭 구분 텍스트 포맷을 사용한다.
     * 값 내부의 역슬래시, 탭, 개행은 이스케이프해서 다음 SELECT 단계가
     * 안정적으로 역직렬화할 수 있도록 저장한다.
     */
    for (cursor = text; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            if (fputs("\\\\", file) == EOF) {
                return 0;
            }
        } else if (*cursor == '\t') {
            if (fputs("\\t", file) == EOF) {
                return 0;
            }
        } else if (*cursor == '\n') {
            if (fputs("\\n", file) == EOF) {
                return 0;
            }
        } else {
            if (fputc(*cursor, file) == EOF) {
                return 0;
            }
        }
    }

    return 1;
}

StorageResult append_storage_row(
    const char *table_name,
    const char *const *values,
    size_t value_count
)
{
    char path_buffer[512];
    size_t index;
    FILE *file;

    if (!build_table_data_path(table_name, path_buffer, sizeof(path_buffer))) {
        return STORAGE_FILE_ERROR;
    }

    file = fopen(path_buffer, "a");
    if (file == NULL) {
        return STORAGE_FILE_ERROR;
    }

    for (index = 0; index < value_count; index++) {
        if (index > 0 && fputc('\t', file) == EOF) {
            fclose(file);
            return STORAGE_FILE_ERROR;
        }

        if (!write_escaped_text(file, values[index])) {
            fclose(file);
            return STORAGE_FILE_ERROR;
        }
    }

    if (fputc('\n', file) == EOF) {
        fclose(file);
        return STORAGE_FILE_ERROR;
    }

    if (fclose(file) != 0) {
        return STORAGE_FILE_ERROR;
    }

    return STORAGE_SUCCESS;
}

StorageResult load_storage_text(const char *table_name, char **out_text)
{
    char path_buffer[512];
    FILE *file;
    char *text;

    if (out_text == NULL) {
        return STORAGE_FILE_ERROR;
    }

    *out_text = NULL;
    if (!build_table_data_path(table_name, path_buffer, sizeof(path_buffer))) {
        return STORAGE_FILE_ERROR;
    }

    file = fopen(path_buffer, "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            return STORAGE_NOT_FOUND;
        }
        return STORAGE_FILE_ERROR;
    }
    fclose(file);

    text = read_entire_file(path_buffer);
    if (text == NULL) {
        return STORAGE_FILE_ERROR;
    }

    *out_text = text;
    return STORAGE_SUCCESS;
}

static StorageResult parse_escaped_field(const char **cursor, char **out_text)
{
    size_t capacity;
    size_t length;
    char *buffer;
    char current;

    capacity = 16;
    length = 0;
    buffer = (char *)malloc(capacity);
    if (buffer == NULL) {
        return STORAGE_FILE_ERROR;
    }

    while (**cursor != '\0' && **cursor != '\t' && **cursor != '\n') {
        current = **cursor;
        (*cursor)++;

        if (current == '\\') {
            if (**cursor == '\0') {
                free(buffer);
                return STORAGE_FORMAT_ERROR;
            }

            current = **cursor;
            (*cursor)++;

            if (current == 't') {
                current = '\t';
            } else if (current == 'n') {
                current = '\n';
            } else if (current == '\\') {
                current = '\\';
            } else {
                free(buffer);
                return STORAGE_FORMAT_ERROR;
            }
        }

        if (length + 1 >= capacity) {
            char *new_buffer;

            capacity *= 2;
            new_buffer = (char *)realloc(buffer, capacity);
            if (new_buffer == NULL) {
                free(buffer);
                return STORAGE_FILE_ERROR;
            }
            buffer = new_buffer;
        }

        buffer[length] = current;
        length++;
    }

    buffer[length] = '\0';
    *out_text = buffer;
    return STORAGE_SUCCESS;
}

StorageResult parse_storage_row(
    const char *line_start,
    size_t expected_count,
    char ***out_fields
)
{
    const char *cursor;
    char **fields;
    size_t index;
    StorageResult result;

    if (line_start == NULL || out_fields == NULL) {
        return STORAGE_FILE_ERROR;
    }

    cursor = line_start;
    fields = (char **)calloc(expected_count, sizeof(char *));
    if (fields == NULL) {
        return STORAGE_FILE_ERROR;
    }

    for (index = 0; index < expected_count; index++) {
        result = parse_escaped_field(&cursor, &fields[index]);
        if (result != STORAGE_SUCCESS) {
            free_string_array(fields, expected_count);
            return result;
        }

        if (index + 1 < expected_count) {
            if (*cursor != '\t') {
                free_string_array(fields, expected_count);
                return STORAGE_FORMAT_ERROR;
            }
            cursor++;
        }
    }

    if (*cursor != '\0' && *cursor != '\n') {
        free_string_array(fields, expected_count);
        return STORAGE_FORMAT_ERROR;
    }

    *out_fields = fields;
    return STORAGE_SUCCESS;
}
