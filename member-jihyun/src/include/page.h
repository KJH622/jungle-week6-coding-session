#ifndef PAGE_H
#define PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "status.h"

#define PAGE_SIZE 4096U
#define PAGE_HEADER_SIZE 16U
#define PAGE_PAYLOAD_SIZE (PAGE_SIZE - PAGE_HEADER_SIZE)
#define PAGE_NO_PAGE 0xffffffffU

typedef struct PageHeader {
    uint32_t page_id;
    uint32_t next_page_id;
    uint32_t row_count;
    uint32_t used_bytes;
} PageHeader;

typedef struct DataPage {
    PageHeader header;
    unsigned char payload[PAGE_PAYLOAD_SIZE];
} DataPage;

typedef struct PageCursor {
    const DataPage *page;
    uint32_t offset;
    uint32_t yielded;
} PageCursor;

void page_init(DataPage *page, uint32_t page_id);
bool page_can_store_record(const DataPage *page, uint32_t record_len);
SqlStatus page_append_record(DataPage *page, const unsigned char *record_data, uint32_t record_len);
void page_cursor_init(PageCursor *cursor, const DataPage *page);
bool page_cursor_next(PageCursor *cursor, const unsigned char **record_data, uint32_t *record_len);

#endif
