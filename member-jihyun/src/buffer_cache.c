#include "include/buffer_cache.h"

#include <stdio.h>
#include <string.h>

/* 프레임이 요청한 파일/페이지와 일치하는지 확인한다. */
static bool frame_matches(const BufferFrame *frame, const char *file_path, uint32_t page_id)
{
    return frame->in_use && frame->page_id == page_id && strcmp(frame->file_path, file_path) == 0;
}

/* dirty 프레임을 원본 파일의 해당 페이지 위치에 기록한다. */
static SqlStatus flush_frame(BufferCache *cache, BufferFrame *frame)
{
    FILE *file;
    long offset;
    size_t written_count;

    if (!frame->in_use || !frame->dirty) {
        return SQL_STATUS_OK;
    }

    file = fopen(frame->file_path, "r+b");
    if (file == NULL) {
        file = fopen(frame->file_path, "w+b");
    }
    if (file == NULL) {
        return SQL_STATUS_FILE_OPEN_FAILED;
    }

    offset = (long)(frame->page_id * PAGE_SIZE);
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return SQL_STATUS_FILE_OPEN_FAILED;
    }

    written_count = fwrite(&frame->page, 1U, sizeof(frame->page), file);
    fclose(file);

    if (written_count != sizeof(frame->page)) {
        return SQL_STATUS_FILE_OPEN_FAILED;
    }

    frame->dirty = false;
    cache->flush_count += 1U;
    return SQL_STATUS_OK;
}

/* 캐시에서 원하는 페이지 프레임을 찾고 LRU 사용 시각을 갱신한다. */
static BufferFrame *find_frame(BufferCache *cache, const char *file_path, uint32_t page_id)
{
    size_t index;

    for (index = 0U; index < BUFFER_CACHE_CAPACITY; index++) {
        if (frame_matches(&cache->frames[index], file_path, page_id)) {
            cache->frames[index].last_used_tick = ++cache->tick;
            return &cache->frames[index];
        }
    }

    return NULL;
}

/* 빈 프레임 또는 LRU 희생 프레임을 선택하고 필요 시 flush한다. */
static BufferFrame *choose_victim(BufferCache *cache, SqlStatus *status)
{
    size_t index;
    BufferFrame *victim;

    for (index = 0U; index < BUFFER_CACHE_CAPACITY; index++) {
        if (!cache->frames[index].in_use) {
            *status = SQL_STATUS_OK;
            return &cache->frames[index];
        }
    }

    victim = &cache->frames[0];
    for (index = 1U; index < BUFFER_CACHE_CAPACITY; index++) {
        if (cache->frames[index].last_used_tick < victim->last_used_tick) {
            victim = &cache->frames[index];
        }
    }

    *status = flush_frame(cache, victim);
    return *status == SQL_STATUS_OK ? victim : NULL;
}

/* 선택된 프레임에 새 파일/페이지 메타데이터를 할당한다. */
static void assign_frame(BufferCache *cache, BufferFrame *frame, const char *file_path, uint32_t page_id)
{
    frame->in_use = true;
    frame->dirty = false;
    frame->page_id = page_id;
    frame->last_used_tick = ++cache->tick;
    strncpy(frame->file_path, file_path, MAX_PATH_LENGTH - 1U);
    frame->file_path[MAX_PATH_LENGTH - 1U] = '\0';
}

/* 버퍼 캐시 전체 상태를 0으로 초기화한다. */
void buffer_cache_init(BufferCache *cache)
{
    memset(cache, 0, sizeof(*cache));
}

/* 파일에서 페이지를 읽어 캐시에 적재하고 페이지 포인터를 반환한다. */
SqlStatus buffer_cache_load_page(BufferCache *cache, const char *file_path, uint32_t page_id, DataPage **out_page)
{
    BufferFrame *frame;
    SqlStatus status;
    FILE *file;
    long offset;
    size_t read_count;

    frame = find_frame(cache, file_path, page_id);
    if (frame != NULL) {
        cache->hit_count += 1U;
        *out_page = &frame->page;
        return SQL_STATUS_OK;
    }
    cache->miss_count += 1U;

    frame = choose_victim(cache, &status);
    if (frame == NULL) {
        return status;
    }

    file = fopen(file_path, "rb");
    if (file == NULL) {
        return SQL_STATUS_FILE_OPEN_FAILED;
    }

    offset = (long)(page_id * PAGE_SIZE);
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return SQL_STATUS_FILE_OPEN_FAILED;
    }

    read_count = fread(&frame->page, 1U, sizeof(frame->page), file);
    fclose(file);

    if (read_count != sizeof(frame->page)) {
        return SQL_STATUS_FILE_OPEN_FAILED;
    }

    assign_frame(cache, frame, file_path, page_id);
    *out_page = &frame->page;
    return SQL_STATUS_OK;
}

/* 새 페이지를 캐시에 생성하고 dirty 상태로 표시한다. */
SqlStatus buffer_cache_create_page(BufferCache *cache, const char *file_path, uint32_t page_id, DataPage **out_page)
{
    BufferFrame *frame;
    SqlStatus status;

    frame = find_frame(cache, file_path, page_id);
    if (frame != NULL) {
        cache->hit_count += 1U;
        page_init(&frame->page, page_id);
        frame->dirty = true;
        *out_page = &frame->page;
        return SQL_STATUS_OK;
    }
    cache->miss_count += 1U;

    frame = choose_victim(cache, &status);
    if (frame == NULL) {
        return status;
    }

    assign_frame(cache, frame, file_path, page_id);
    page_init(&frame->page, page_id);
    frame->dirty = true;
    *out_page = &frame->page;
    return SQL_STATUS_OK;
}

/* 지정한 페이지 프레임을 수정됨(dirty) 상태로 바꾼다. */
void buffer_cache_mark_dirty(BufferCache *cache, const char *file_path, uint32_t page_id)
{
    BufferFrame *frame;

    frame = find_frame(cache, file_path, page_id);
    if (frame != NULL) {
        frame->dirty = true;
    }
}

/* 특정 파일에 속한 dirty 프레임만 골라 디스크에 반영한다. */
SqlStatus buffer_cache_flush_file(BufferCache *cache, const char *file_path)
{
    size_t index;
    SqlStatus status;

    for (index = 0U; index < BUFFER_CACHE_CAPACITY; index++) {
        if (cache->frames[index].in_use && strcmp(cache->frames[index].file_path, file_path) == 0) {
            status = flush_frame(cache, &cache->frames[index]);
            if (status != SQL_STATUS_OK) {
                return status;
            }
        }
    }

    return SQL_STATUS_OK;
}

/* 캐시 전체 프레임을 순회하며 dirty 페이지를 모두 flush한다. */
SqlStatus buffer_cache_flush_all(BufferCache *cache)
{
    size_t index;
    SqlStatus status;

    for (index = 0U; index < BUFFER_CACHE_CAPACITY; index++) {
        status = flush_frame(cache, &cache->frames[index]);
        if (status != SQL_STATUS_OK) {
            return status;
        }
    }

    return SQL_STATUS_OK;
}

void buffer_cache_get_stats(
    const BufferCache *cache,
    uint64_t *hit_count,
    uint64_t *miss_count,
    uint64_t *dirty_pages,
    uint64_t *flush_count
)
{
    size_t index;
    uint64_t dirty_count;

    if (cache == NULL) {
        return;
    }

    dirty_count = 0U;
    for (index = 0U; index < BUFFER_CACHE_CAPACITY; index++) {
        if (cache->frames[index].in_use && cache->frames[index].dirty) {
            dirty_count += 1U;
        }
    }

    if (hit_count != NULL) {
        *hit_count = cache->hit_count;
    }
    if (miss_count != NULL) {
        *miss_count = cache->miss_count;
    }
    if (dirty_pages != NULL) {
        *dirty_pages = dirty_count;
    }
    if (flush_count != NULL) {
        *flush_count = cache->flush_count;
    }
}
