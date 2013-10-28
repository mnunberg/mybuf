#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "mybuf.h"

void test1(void)
{
    int ii;
    int test_length = 10000;
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
    return 0;
}
