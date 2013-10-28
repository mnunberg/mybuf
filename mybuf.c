#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>

#include "mybuf.h"


/** Default buffer allocation size */
#define BUFFER_ALLOC_INIT 1024


void
mybuf_contig1_init(mybuf_contig1_t *buf)
{
    buf->data = malloc(BUFFER_ALLOC_INIT);
    buf->alloc = 1024;
    buf->length = 0;
    buf->start_offset = 0;
}

void
mybuf_contig1_cleanup(mybuf_contig1_t *buf)
{
    free(buf->data);
    memset(buf, 0, sizeof(*buf));
}


void *
mybuf_contig1_get_segment(mybuf_contig1_t *buf, unsigned long size)
{
    void *ret;

    if (MYBUF_CONTIG1_SPACE(buf) >= size) {
        ret = MYBUF_CONTIG1_TAIL(buf);
        buf->length += size;
        return ret;
    }

    if (MYBUF_CONTIG1_MAXSPACE(buf) >= size) {
        mybuf_contig1_compact(buf);
        return mybuf_contig1_get_segment(buf, size);
    }

    while (MYBUF_CONTIG1_SPACE(buf) < size) {
        buf->alloc *= 2;
    }

    buf->data = realloc(buf->data, buf->alloc);
    return mybuf_contig1_get_segment(buf, size);
}

/** Appends data to the end of the buffer */
void
mybuf_contig1_append(mybuf_contig1_t *buf,
                     const void *data, unsigned long ndata)
{
    void *mem = mybuf_contig1_get_segment(buf, ndata);
    memcpy(mem, data, ndata);
}


void
mybuf_contig1_compact(mybuf_contig1_t *buf)
{
    /** Figure out whether to use memcpy or memmove. memcpy is quicker */
    if (buf->data + buf->length < MYBUF_CONTIG1_HEAD(buf)) {
        memcpy(buf->data, MYBUF_CONTIG1_HEAD(buf), buf->length);
    } else {
        memmove(buf->data, MYBUF_CONTIG1_HEAD(buf), buf->length);
    }
    buf->start_offset = 0;
}

void
mybuf_contig1_chop_nocompact(mybuf_contig1_t *buf, unsigned long offset)
{
    buf->start_offset += offset;
    buf->length -= offset;
}

void
mybuf_contig1_chop(mybuf_contig1_t *buf, unsigned long offset)
{
    mybuf_contig1_chop_nocompact(buf, offset);
    if (buf->start_offset > buf->alloc / 2) {
        mybuf_contig1_compact(buf);
    }
}

void
mybuf_regpool_init(mybuf_regpool_t *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->pinned = 0;
    lcb_list_init(&pool->regions.ll);
    lcb_list_init(&pool->flushed_regions.ll);
    mybuf_contig1_init(&pool->buf);
}

void
mybuf_regpool_clean(mybuf_regpool_t *pool)\
{
    mybuf_contig1_cleanup(&pool->buf);
}

static void
update_single_region(mybuf_regpool_t *pool, mybuf_region_t *cur,
                     unsigned long old_offset, char *old_buffer)
{
    unsigned long old_begin;
    if (cur->flags & MYBUF_REGION_F_ALLOCATED) {
        return; /* don't care */
    }

    /** Offset into the old buffer */
    old_begin = cur->buf - old_buffer;
    cur->buf = pool->buf.data + old_begin;

    /** Did the old one have an offset? */
    if (old_offset) {
        if (old_offset != pool->buf.start_offset) {
            if (old_offset > pool->buf.start_offset) {
                cur->buf -= (old_offset - pool->buf.start_offset);
            } else {
                cur->buf += (pool->buf.start_offset - old_offset);
            }
        } else {
            cur->buf += old_offset;
        }
    } else if (pool->buf.start_offset) {
        cur->buf += pool->buf.start_offset;
    }
}

static void
update_region_offsets(mybuf_regpool_t *pool,
                      unsigned long old_offset,
                      char *old_buffer)
{
    lcb_list_t *cur_ll;
    LCB_LIST_FOR(cur_ll, &pool->regions.ll) {
        mybuf_region_t *cur = LCB_LIST_ITEM(cur_ll, mybuf_region_t, ll);
        update_single_region(pool, cur, old_offset, old_buffer);
    }

    LCB_LIST_FOR(cur_ll, &pool->flushed_regions.ll) {
        mybuf_region_t *cur = LCB_LIST_ITEM(cur_ll, mybuf_region_t, ll);
        update_single_region(pool, cur, old_offset, old_buffer);
    }
}

void
mybuf_regpool_get_region(mybuf_regpool_t *pool, unsigned long size,
                         mybuf_region_t **region)
{
    /**
     * SCENARIO:
     * Enough free space within the buffer (without compaction/realloc)
     * ACTION:
     * Allocate the segment and return it
     */
    if (!*region) {
        *region = calloc(1, sizeof(**region));

    } else {
        (*region)->flags |= MYBUF_REGION_F_STRUCTUALLOC;
    }

    (*region)->length = size;

    if (MYBUF_CONTIG1_SPACE(&pool->buf) >= size) {
        (*region)->buf = mybuf_contig1_get_segment(&pool->buf, size);

    } else {

        if (!pool->pinned) {
            void *mem, *old_base;
            unsigned long old_offset;

            old_base = pool->buf.data;
            old_offset = pool->buf.start_offset;

            mem = mybuf_contig1_get_segment(&pool->buf, size);
            update_region_offsets(pool, old_offset, old_base);
            (*region)->buf = mem;

        } else {
            (*region)->flags |= MYBUF_REGION_F_ALLOCATED;
            (*region)->buf = malloc(size);
        }
    }

    lcb_list_append(&pool->regions.ll, &(*region)->ll);
}


void
mybuf_regpool_pin(mybuf_regpool_t *pool, mybuf_region_t *region)
{
    if (region->flags & (MYBUF_REGION_F_ALLOCATED|MYBUF_REGION_F_PINNED)) {
        return;
    }

    region->flags |= MYBUF_REGION_F_PINNED;
    pool->pinned++;
}

void
mybuf_regpool_unpin(mybuf_regpool_t *pool, mybuf_region_t *region)
{
    if (region->flags & MYBUF_REGION_F_ALLOCATED) {
        return;
    }
    assert(region->flags & MYBUF_REGION_F_PINNED);
    region->flags &= (~MYBUF_REGION_F_PINNED);
    pool->pinned--;
}

void
mybuf_regpool_free_region(mybuf_regpool_t *pool, mybuf_region_t *region)
{
    assert( (region->flags & MYBUF_REGION_F_PINNED) == 0);

    if (region->flags & MYBUF_REGION_F_ALLOCATED) {
        free(region->buf);

    } else if (region->buf == MYBUF_CONTIG1_HEAD(&pool->buf)) {
        mybuf_contig1_chop_nocompact(&pool->buf, region->length);
    }

    lcb_list_delete(&region->ll);

    if ((region->flags & MYBUF_REGION_F_STRUCTUALLOC) == 0) {
        free(region);
    }
}

void
mybuf_regpool_iov_get(mybuf_regpool_t *pool, mybuf_generic_iov *iov,
                      unsigned int niov)
{
    /**
     * Normally only one structure will be needed if there are no 'holes' in
     * the buffer.
     *
     * If there are holes, each contiguous chunk will occupy a single IOV
     */
    lcb_list_t *cur_ll;
    mybuf_generic_iov *iov_cur = iov, *iov_end = (iov + (niov + 1));
    unsigned int flush_offset;
    void *expected_pos = NULL;

    flush_offset = pool->flush_offset;

    LCB_LIST_FOR(cur_ll, &pool->regions.ll) {
        mybuf_region_t *cur = LCB_LIST_ITEM(cur_ll, mybuf_region_t, ll);

        if (!expected_pos) {
            /** First time around */

            GT_NEW_IOV:
            expected_pos = cur->buf + cur->length;
            iov_cur->iov_base = cur->buf;
            iov_cur->iov_len = cur->length;

            if (flush_offset) {
                assert(flush_offset < cur->length);
                iov_cur->iov_base += flush_offset;
                iov_cur->iov_len -= flush_offset;
                flush_offset = 0;
            }

        } else if (cur->buf == expected_pos) {
            /** Still in the same chunk. Just increase the length */
            iov_cur->iov_len += cur->length;
            expected_pos = cur->buf + cur->length;

        } else {
            /** Request a new chunk */
            iov_cur++;
            expected_pos = NULL;
            if (iov_cur == iov_end) {
                break;
            }
            goto GT_NEW_IOV;
        }
    }

    if (!expected_pos) {
        /** Indicator that we have nothing in the buffer */
        iov->iov_len = 0;
        iov->iov_base = 0;
    }

    pool->pinned++;
}

void
mybuf_regpool_iov_done(mybuf_regpool_t *pool, unsigned long nused)
{
    pool->pinned--;
    lcb_list_t *cur_ll;

    nused += pool->flush_offset;
    pool->flush_offset = 0;

    while ( nused && (cur_ll = lcb_list_shift(&pool->regions.ll)) ) {
        mybuf_region_t *cur = LCB_LIST_ITEM(cur_ll, mybuf_region_t, ll);
        if (nused >= cur->length) {
            cur->flags |= MYBUF_REGION_F_FLUSHED;
            nused -= cur->length;
            lcb_list_append(&pool->flushed_regions.ll, cur_ll);

        } else {
            pool->flush_offset = cur->length - nused;
            lcb_list_prepend(&pool->regions.ll, cur_ll);
            break;
        }
    }
}
