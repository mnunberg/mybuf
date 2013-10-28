#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "mybuf.h"

void test2(void)
{
    mybuf_regpool_t pool;
    mybuf_region_t *region = NULL;
    mybuf_generic_iov iov;

    mybuf_regpool_init(&pool);

    mybuf_regpool_get_region(&pool, 1024, &region);

    assert(region);
    assert(region->buf);
    assert(region->length == 1024);
    assert(region->flags == 0);

    mybuf_regpool_iov_get(&pool, &iov, 1);
    assert(iov.iov_base == region->buf);
    assert(iov.iov_len == region->length);

    mybuf_regpool_iov_done(&pool, 512);
    assert(pool.flush_offset == 512);

    mybuf_regpool_iov_get(&pool, &iov, 1);
    assert(iov.iov_base == region->buf + 512);
    assert(iov.iov_len == 512);
    mybuf_regpool_iov_done(&pool, 512);
    assert(pool.flush_offset == 0);

    mybuf_regpool_free_region(&pool, region);
    mybuf_regpool_clean(&pool);
}

void test3(void)
{
    unsigned int ii;
    mybuf_region_t regions[10];
    mybuf_regpool_t pool;
    mybuf_generic_iov iov;

    memset(regions, 0, sizeof(regions));
    mybuf_regpool_init(&pool);
    for (ii = 0; ii < 10; ii++) {
        mybuf_region_t *pp = regions + ii;
        mybuf_regpool_get_region(&pool, 4096, &pp);
        assert(pp);
        assert(pp->flags == MYBUF_REGION_F_STRUCTUALLOC);
        assert(pp->buf);
        assert(pp->length == 4096);

        /** Fill the buffer with data */
        memset(pp->buf, ii, pp->length);
    }

    for (ii = 0; ii < 10; ii++) {
        char buf[4096];
        memset(buf, ii, sizeof(buf));
        assert(memcmp(buf, regions[ii].buf, sizeof(buf)) == 0);
    }

    /** delete the first region */
    mybuf_regpool_free_region(&pool, regions + 0);

    mybuf_regpool_iov_get(&pool, &iov, 1);
    assert(iov.iov_base == regions[1].buf);
    assert(iov.iov_len == 4096 * 9);

    /**
     * Make sure the contents are what we expect
     */
    for (ii = 1; ii < 10; ii++) {
        char buf[4096];
        memset(buf, ii, sizeof(buf));
        assert(memcmp(buf, iov.iov_base, sizeof(buf)) == 0);
        mybuf_regpool_iov_done(&pool, 4096);
        mybuf_regpool_iov_get(&pool, &iov, 1);
    }

    assert(iov.iov_len == 0);

    for (ii = 1; ii < 10; ii++) {
        mybuf_regpool_free_region(&pool, regions + ii);
    }

    mybuf_regpool_clean(&pool);
}

void test1(void)
{
    unsigned int ii;
    unsigned int test_length = 10000;
    char buf[1024] = { '*' };

    mybuf_contig1_t mb = { NULL };
    mybuf_contig1_init(&mb);

    for (ii = 0; ii < test_length; ii++) {
        mybuf_contig1_append(&mb, buf, 1);
    }

    assert(mb.length == test_length);
    assert(mb.start_offset == 0);
    assert(mb.alloc >= test_length);

    mybuf_contig1_chop(&mb, 200);
    assert(mb.length == test_length - 200);
    assert(mb.start_offset == 200);

    mybuf_contig1_compact(&mb);
    assert(mb.start_offset == 0);

    mybuf_contig1_cleanup(&mb);
}

int main(void)
{
    test1();
    test2();
    test3();
    return 0;
}
