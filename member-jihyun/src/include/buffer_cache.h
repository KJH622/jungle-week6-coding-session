#ifndef BUFFER_CACHE_H
#define BUFFER_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "page.h"
#include "status.h"

#define BUFFER_CACHE_CAPACITY 4U
#define MAX_PATH_LENGTH 512U

typedef struct BufferFrame {
    bool in_use;
    bool dirty;
    uint64_t last_used_tick;
    char file_path[MAX_PATH_LENGTH];
    uint32_t page_id;
    DataPage page;
} BufferFrame;

typedef struct BufferCache {
    BufferFrame frames[BUFFER_CACHE_CAPACITY];
    uint64_t tick;
    uint64_t hit_count;
    uint64_t miss_count;
    uint64_t flush_count;
} BufferCache;

void buffer_cache_init(BufferCache *cache);
SqlStatus buffer_cache_load_page(BufferCache *cache, const char *file_path, uint32_t page_id, DataPage **out_page);
SqlStatus buffer_cache_create_page(BufferCache *cache, const char *file_path, uint32_t page_id, DataPage **out_page);
void buffer_cache_mark_dirty(BufferCache *cache, const char *file_path, uint32_t page_id);
SqlStatus buffer_cache_flush_file(BufferCache *cache, const char *file_path);
SqlStatus buffer_cache_flush_all(BufferCache *cache);
void buffer_cache_get_stats(
    const BufferCache *cache,
    uint64_t *hit_count,
    uint64_t *miss_count,
    uint64_t *dirty_pages,
    uint64_t *flush_count
);

#endif
