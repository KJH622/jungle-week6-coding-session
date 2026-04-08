#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

/*
 * 파일 전체를 한 번에 읽어서 NUL 종료된 문자열로 반환한다.
 * 호출한 쪽에서 free()로 해제해야 하며, 실패 시 NULL을 반환한다.
 */
char *read_entire_file(const char *path);

/*
 * 문자열 앞뒤의 공백 문자를 제거한다.
 * 원본 버퍼를 제자리에서 수정하며, 실제 내용이 시작하는 위치를 반환한다.
 */
char *trim_in_place(char *text);

/*
 * 주어진 문자열을 새로 복사해서 반환한다.
 * 표준 strdup 의존성을 줄이기 위해 직접 제공한다.
 */
char *duplicate_string(const char *text);

/*
 * 동적 문자열 배열을 해제한다.
 * count 개수만큼 각 원소를 free() 한 뒤 배열 자체도 free() 한다.
 */
void free_string_array(char **items, size_t count);

#endif
