#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>

typedef enum StorageResult {
    STORAGE_SUCCESS = 0,
    STORAGE_NOT_FOUND = 1,
    STORAGE_FILE_ERROR = 2,
    STORAGE_FORMAT_ERROR = 3
} StorageResult;

/*
 * 테이블 데이터 파일 경로를 현재 작업 디렉토리 기준 상대 경로로 만든다.
 * 성공 시 path_buffer 에 "<table>.data"를 채운다.
 */
int build_table_data_path(const char *table_name, char *path_buffer, size_t path_size);

/*
 * 스키마 순서로 정렬된 문자열 값 목록을 테이블 데이터 파일에 한 줄 append 한다.
 * 값 내부의 역슬래시, 탭, 개행은 저장 포맷 규칙에 맞게 이스케이프한다.
 */
StorageResult append_storage_row(
    const char *table_name,
    const char *const *values,
    size_t value_count
);

/*
 * 테이블 데이터 파일 전체를 읽어 반환한다.
 * 파일이 아직 없으면 STORAGE_NOT_FOUND 를 반환한다.
 * 성공 시 out_text 는 free()로 해제해야 한다.
 */
StorageResult load_storage_text(const char *table_name, char **out_text);

/*
 * 저장 포맷 한 줄을 필드 배열로 역직렬화한다.
 * 성공 시 out_fields 는 free_string_array()로 해제해야 한다.
 */
StorageResult parse_storage_row(
    const char *line_start,
    size_t expected_count,
    char ***out_fields
);

#endif
