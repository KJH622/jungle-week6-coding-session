#include "include/storage.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/page.h"
#include "include/utils.h"

/* 테이블 이름으로 실제 데이터 파일 경로를 생성한다. */
static void build_table_path(const StorageEngine *engine, const char *table_name, char *path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s.data", engine->data_dir, table_name);
}

/* 테이블 데이터 파일이 없으면 빈 파일을 생성한다. */
static SqlStatus ensure_table_file(const char *path)
{
    FILE *file;

    file = fopen(path, "ab");
    if (file == NULL) {
        return SQL_STATUS_FILE_OPEN_FAILED;
    }

    fclose(file);
    return SQL_STATUS_OK;
}

/* 파일 크기를 기준으로 현재 페이지 개수를 계산한다. */
static SqlStatus get_page_count(const char *path, uint32_t *page_count)
{
    FILE *file;
    long file_size;

    file = fopen(path, "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            *page_count = 0U;
            return SQL_STATUS_OK;
        }
        return SQL_STATUS_FILE_OPEN_FAILED;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        return SQL_STATUS_FILE_OPEN_FAILED;
    }

    file_size = ftell(file);
    fclose(file);
    if (file_size < 0L) {
        return SQL_STATUS_FILE_OPEN_FAILED;
    }

    *page_count = (uint32_t)((unsigned long)file_size / PAGE_SIZE);
    return SQL_STATUS_OK;
}

/* 한 행의 문자열 값을 페이지 저장용 바이너리 레코드로 직렬화한다. */
static SqlStatus serialize_row(const TableSchema *schema, char **values, unsigned char **out_buffer, uint32_t *out_length)
{
    size_t total_length;
    size_t index;
    unsigned char *buffer;
    unsigned char *cursor;
    uint32_t field_count;

    total_length = sizeof(uint32_t);
    for (index = 0U; index < schema->column_count; index++) {
        total_length += sizeof(uint32_t) + strlen(values[index]);
    }

    buffer = (unsigned char *)malloc(total_length);
    if (buffer == NULL) {
        return SQL_STATUS_INTERNAL_ERROR;
    }

    cursor = buffer;
    field_count = (uint32_t)schema->column_count;
    memcpy(cursor, &field_count, sizeof(field_count));
    cursor += sizeof(field_count);

    for (index = 0U; index < schema->column_count; index++) {
        uint32_t field_length;

        field_length = (uint32_t)strlen(values[index]);
        memcpy(cursor, &field_length, sizeof(field_length));
        cursor += sizeof(field_length);
        memcpy(cursor, values[index], field_length);
        cursor += field_length;
    }

    *out_buffer = buffer;
    *out_length = (uint32_t)total_length;
    return SQL_STATUS_OK;
}

/* 역직렬화 중간에 할당된 값 배열을 부분적으로 정리한다. */
static void free_partial_values(char **values, size_t count)
{
    size_t index;

    if (values == NULL) {
        return;
    }

    for (index = 0U; index < count; index++) {
        free(values[index]);
    }

    free(values);
}

/* 바이너리 레코드를 RowData 구조체로 역직렬화한다. */
static SqlStatus deserialize_row(const unsigned char *record_data, uint32_t record_length, RowData *out_row)
{
    const unsigned char *cursor;
    const unsigned char *record_end;
    uint32_t field_count;
    char **values;
    size_t index;

    if (record_length < sizeof(uint32_t)) {
        return SQL_STATUS_INVALID_QUERY;
    }

    cursor = record_data;
    record_end = record_data + record_length;
    memcpy(&field_count, cursor, sizeof(field_count));
    cursor += sizeof(field_count);

    values = (char **)calloc(field_count, sizeof(char *));
    if (values == NULL) {
        return SQL_STATUS_INTERNAL_ERROR;
    }

    for (index = 0U; index < field_count; index++) {
        uint32_t field_length;

        if ((size_t)(record_end - cursor) < sizeof(field_length)) {
            free_partial_values(values, index);
            return SQL_STATUS_INVALID_QUERY;
        }

        memcpy(&field_length, cursor, sizeof(field_length));
        cursor += sizeof(field_length);

        if ((size_t)(record_end - cursor) < field_length) {
            free_partial_values(values, index);
            return SQL_STATUS_INVALID_QUERY;
        }

        values[index] = util_strndup((const char *)cursor, field_length);
        if (values[index] == NULL) {
            free_partial_values(values, index);
            return SQL_STATUS_INTERNAL_ERROR;
        }

        cursor += field_length;
    }

    out_row->value_count = field_count;
    out_row->values = values;
    return SQL_STATUS_OK;
}

/* 스토리지 엔진과 버퍼 캐시를 초기화한다. */
SqlStatus storage_engine_init(StorageEngine *engine, const char *data_dir)
{
    memset(engine, 0, sizeof(*engine));
    strncpy(engine->data_dir, data_dir, MAX_PATH_LENGTH - 1U);
    engine->data_dir[MAX_PATH_LENGTH - 1U] = '\0';
    buffer_cache_init(&engine->cache);
    return SQL_STATUS_OK;
}

/* 스토리지 엔진 종료 시 캐시된 페이지를 모두 flush한다. */
void storage_engine_destroy(StorageEngine *engine)
{
    if (engine == NULL) {
        return;
    }

    buffer_cache_flush_all(&engine->cache);
}

/* 직렬화한 레코드를 마지막 페이지(또는 새 페이지)에 append한다. */
SqlStatus storage_insert_row(StorageEngine *engine, const TableSchema *schema, char **values)
{
    char path[MAX_PATH_LENGTH];
    unsigned char *record_data;
    uint32_t record_length;
    uint32_t page_count;
    uint32_t target_page_id;
    DataPage *page;
    SqlStatus status;

    build_table_path(engine, schema->table_name, path, sizeof(path));

    status = ensure_table_file(path);
    if (status != SQL_STATUS_OK) {
        return status;
    }

    status = get_page_count(path, &page_count);
    if (status != SQL_STATUS_OK) {
        return status;
    }

    status = serialize_row(schema, values, &record_data, &record_length);
    if (status != SQL_STATUS_OK) {
        return status;
    }

    if (page_count == 0U) {
        status = buffer_cache_create_page(&engine->cache, path, 0U, &page);
        if (status != SQL_STATUS_OK) {
            free(record_data);
            return status;
        }
        page_count = 1U;
        target_page_id = 0U;
    } else {
        target_page_id = page_count - 1U;
        status = buffer_cache_load_page(&engine->cache, path, target_page_id, &page);
        if (status != SQL_STATUS_OK) {
            free(record_data);
            return status;
        }
    }

    if (!page_can_store_record(page, record_length)) {
        DataPage *new_page;

        page->header.next_page_id = page_count;
        buffer_cache_mark_dirty(&engine->cache, path, target_page_id);

        status = buffer_cache_create_page(&engine->cache, path, page_count, &new_page);
        if (status != SQL_STATUS_OK) {
            free(record_data);
            return status;
        }

        page = new_page;
        target_page_id = page_count;
    }

    status = page_append_record(page, record_data, record_length);
    free(record_data);
    if (status != SQL_STATUS_OK) {
        return status;
    }

    buffer_cache_mark_dirty(&engine->cache, path, target_page_id);
    return buffer_cache_flush_file(&engine->cache, path);
}

/* 테이블의 모든 레코드를 순회하며 방문자 콜백으로 전달한다. */
SqlStatus storage_scan_rows(StorageEngine *engine, const TableSchema *schema, RowVisitor visitor, void *context)
{
    char path[MAX_PATH_LENGTH];
    uint32_t page_count;
    uint32_t page_id;
    SqlStatus status;

    build_table_path(engine, schema->table_name, path, sizeof(path));
    status = get_page_count(path, &page_count);
    if (status != SQL_STATUS_OK || page_count == 0U) {
        return status;
    }

    for (page_id = 0U; page_id < page_count; page_id++) {
        DataPage *page;
        PageCursor cursor;
        const unsigned char *record_data;
        uint32_t record_length;

        status = buffer_cache_load_page(&engine->cache, path, page_id, &page);
        if (status != SQL_STATUS_OK) {
            return status;
        }

        page_cursor_init(&cursor, page);
        while (page_cursor_next(&cursor, &record_data, &record_length)) {
            RowData row;
            bool should_continue;

            status = deserialize_row(record_data, record_length, &row);
            if (status != SQL_STATUS_OK) {
                return status;
            }

            should_continue = visitor(&row, context);
            row_data_destroy(&row);

            if (!should_continue) {
                return SQL_STATUS_OK;
            }
        }
    }

    return SQL_STATUS_OK;
}

/* 버퍼 캐시의 모든 dirty 페이지를 디스크에 반영한다. */
SqlStatus storage_flush(StorageEngine *engine)
{
    return buffer_cache_flush_all(&engine->cache);
}

void storage_get_cache_stats(
    const StorageEngine *engine,
    uint64_t *hit_count,
    uint64_t *miss_count,
    uint64_t *dirty_pages,
    uint64_t *flush_count
)
{
    if (engine == NULL) {
        return;
    }

    buffer_cache_get_stats(&engine->cache, hit_count, miss_count, dirty_pages, flush_count);
}

/* RowData가 소유한 값 배열을 해제한다. */
void row_data_destroy(RowData *row)
{
    size_t index;

    if (row == NULL || row->values == NULL) {
        return;
    }

    for (index = 0U; index < row->value_count; index++) {
        free(row->values[index]);
    }

    free(row->values);
    row->values = NULL;
    row->value_count = 0U;
}
