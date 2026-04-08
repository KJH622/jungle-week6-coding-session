#include "schema.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 파일 이름이 ".schema"로 끝나는지 확인합니다. */
static int has_schema_extension(const char *name) {
    size_t len = strlen(name);
    return len >= 7 && strcmp(name + len - 7, ".schema") == 0;
}

/* 스키마 파일 전체를 문자열 버퍼 하나에 읽어옵니다.
   이 프로그램의 스키마 파서는 문자열 검색으로 동작합니다. */
static int read_file_text(const char *path, char *buffer, size_t size) {
    FILE *fp = fopen(path, "r");
    size_t read_len;

    if (fp == NULL) {
        return -1;
    }

    read_len = fread(buffer, 1, size - 1, fp);
    if (ferror(fp)) {
        fclose(fp);
        return -1;
    }

    buffer[read_len] = '\0';
    fclose(fp);
    return 0;
}

/* JSON 키 뒤에 있는 문자열 값을 하나 꺼냅니다.
   예를 들어 "table" 근처를 넘기면 "users"를 뽑아냅니다. */
static int extract_json_string(const char *key_pos, char *out, size_t out_size) {
    const char *colon = strchr(key_pos, ':');
    const char *start;
    const char *end;
    size_t len;

    if (colon == NULL) {
        return -1;
    }

    start = strchr(colon, '"');
    if (start == NULL) {
        return -1;
    }
    start++;

    end = strchr(start, '"');
    if (end == NULL) {
        return -1;
    }

    len = (size_t)(end - start);
    if (len >= out_size) {
        return -1;
    }

    memcpy(out, start, len);
    out[len] = '\0';
    return 0;
}

/* 스키마 파일의 원본 문자열을 TableDef 구조체 1개로 바꿉니다.
   외부 JSON 라이브러리를 쓰지 않기 때문에 직접 간단한 파서를 만들었습니다. */
static int parse_schema_content(const char *content, TableDef *table) {
    const char *table_key = strstr(content, "\"table\"");
    const char *columns_key = strstr(content, "\"columns\"");
    const char *array_start;
    const char *array_end;
    const char *cursor;

    memset(table, 0, sizeof(*table));

    /* 먼저 테이블 이름을 찾습니다. */
    if (table_key == NULL || extract_json_string(table_key, table->name, sizeof(table->name)) != 0) {
        return -1;
    }

    if (columns_key == NULL) {
        return -1;
    }

    array_start = strchr(columns_key, '[');
    array_end = strchr(columns_key, ']');
    if (array_start == NULL || array_end == NULL || array_end <= array_start) {
        return -1;
    }

    /* 그다음 columns 배열 안을 돌면서 컬럼 정보를 하나씩 읽습니다. */
    cursor = array_start;
    while (cursor < array_end) {
        const char *name_key = strstr(cursor, "\"name\"");
        const char *type_key;

        if (name_key == NULL || name_key >= array_end) {
            break;
        }

        if (table->column_count >= MAX_COLUMNS) {
            return -1;
        }

        type_key = strstr(name_key, "\"type\"");
        if (type_key == NULL || type_key >= array_end) {
            return -1;
        }

        /* 읽은 컬럼 이름과 타입을 다음 빈 칸에 저장합니다. */
        if (extract_json_string(name_key,
                                table->columns[table->column_count].name,
                                sizeof(table->columns[table->column_count].name)) != 0) {
            return -1;
        }

        if (extract_json_string(type_key,
                                table->columns[table->column_count].type,
                                sizeof(table->columns[table->column_count].type)) != 0) {
            return -1;
        }

        table->column_count++;
        /* 다음 컬럼을 찾기 위해 검색 위치를 앞으로 옮깁니다. */
        cursor = type_key + 6;
    }

    return table->column_count > 0 ? 0 : -1;
}

/* 현재 디렉토리의 모든 .schema 파일을 읽습니다.
   성공한 파일 1개는 tables 배열의 TableDef 1개가 됩니다. */
int load_schemas(const char *dir, TableDef *tables, int max_tables) {
    DIR *dp = opendir(dir);
    struct dirent *entry;
    int table_count = 0;

    if (dp == NULL) {
        return -1;
    }

    while ((entry = readdir(dp)) != NULL) {
        char path[1024];
        char content[8192];

        /* 스키마 파일이 아니면 건너뜁니다. */
        if (!has_schema_extension(entry->d_name)) {
            continue;
        }

        if (table_count >= max_tables) {
            closedir(dp);
            return -1;
        }

        if (snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name) >= (int)sizeof(path)) {
            closedir(dp);
            return -1;
        }

        if (read_file_text(path, content, sizeof(content)) != 0) {
            closedir(dp);
            return -1;
        }

        if (parse_schema_content(content, &tables[table_count]) != 0) {
            closedir(dp);
            return -1;
        }

        /* 테이블 1개를 읽었으니 다음 칸으로 이동합니다. */
        table_count++;
    }

    closedir(dp);
    return table_count;
}
