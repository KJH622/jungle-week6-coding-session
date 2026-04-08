#include "include/page.h"

#include <string.h>

/* 빈 페이지 구조를 초기화하고 페이지 식별자를 설정한다. */
void page_init(DataPage *page, uint32_t page_id)
{
    memset(page, 0, sizeof(*page));
    page->header.page_id = page_id;
    page->header.next_page_id = PAGE_NO_PAGE;
}

/* 현재 페이지 여유 공간으로 레코드 저장 가능 여부를 판단한다. */
bool page_can_store_record(const DataPage *page, uint32_t record_len)
{
    uint32_t required;

    required = (uint32_t)sizeof(uint32_t) + record_len;
    return page->header.used_bytes + required <= PAGE_PAYLOAD_SIZE;
}

/* 페이지 payload 끝에 길이+레코드 데이터를 순차적으로 추가한다. */
SqlStatus page_append_record(DataPage *page, const unsigned char *record_data, uint32_t record_len)
{
    uint32_t offset;

    if (!page_can_store_record(page, record_len)) {
        return SQL_STATUS_INTERNAL_ERROR;
    }

    offset = page->header.used_bytes;
    memcpy(page->payload + offset, &record_len, sizeof(record_len));
    offset += (uint32_t)sizeof(record_len);
    memcpy(page->payload + offset, record_data, record_len);

    page->header.used_bytes += (uint32_t)sizeof(record_len) + record_len;
    page->header.row_count += 1U;

    return SQL_STATUS_OK;
}

/* 페이지 내 레코드 순회를 위한 커서를 초기 상태로 만든다. */
void page_cursor_init(PageCursor *cursor, const DataPage *page)
{
    cursor->page = page;
    cursor->offset = 0U;
    cursor->yielded = 0U;
}

/* 다음 레코드 포인터와 길이를 반환하고 커서를 전진시킨다. */
bool page_cursor_next(PageCursor *cursor, const unsigned char **record_data, uint32_t *record_len)
{
    uint32_t length;

    if (cursor->yielded >= cursor->page->header.row_count || cursor->offset >= cursor->page->header.used_bytes) {
        return false;
    }

    memcpy(&length, cursor->page->payload + cursor->offset, sizeof(length));
    cursor->offset += (uint32_t)sizeof(length);

    *record_len = length;
    *record_data = cursor->page->payload + cursor->offset;

    cursor->offset += length;
    cursor->yielded += 1U;
    return true;
}
