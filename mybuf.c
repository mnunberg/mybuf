#include <stdlib.h>
#include <string.h>
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

/** Appends data to the end of the buffer */
void
mybuf_contig1_append(mybuf_contig1_t *buf,
                     const void *data, unsigned long ndata)
{
    if (MYBUF_CONTIG1_SPACE(buf) >= ndata) {
        memcpy(buf->data + buf->start_offset + buf->length, data, ndata);
        buf->length += ndata;
        return;
    }

    if (MYBUF_CONTIG1_MAXSPACE(buf) >= ndata) {
        mybuf_contig1_compact(buf);
        mybuf_contig1_append(buf, data, ndata);
        return;
    }

    while (MYBUF_CONTIG1_SPACE(buf) < ndata) {
        buf->alloc *= 2;
    }

    buf->data = realloc(buf->data, buf->alloc);
    mybuf_contig1_append(buf, data, ndata);
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
mybuf_contig1_chop(mybuf_contig1_t *buf, unsigned long offset)
{
    buf->start_offset += offset;
    buf->length -= offset;

    if (buf->start_offset > buf->alloc / 2) {
        mybuf_contig1_compact(buf);
    }
}
