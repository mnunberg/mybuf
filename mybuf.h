#ifndef MYBUF_H
#define MYBUF_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Structures are labelled with a name and a number, in case more specialized
 * buffer types arise. Currently there's only a "contig1" type which is a
 * contiguous buffer with dynamic resizing and compacting features
 */

/**
 * Simple contiguous buffer. In addition to dynamic resizing upon 'append',
 * this also features "chop" functionality which allows trimming the effective
 * size from the *beginning* of the buffer, compacting space if necessary
 */
typedef struct {
    /** Pointer to allocated memory */
    char *data;

    /** Size of allocated memory */
    unsigned long alloc;

    /** Offset of the beginning of the buffer */
    unsigned long start_offset;

    /** Length of used size of the buffer */
    unsigned long length;
} mybuf_contig1_t;

/** Space inside the buffer */
#define MYBUF_CONTIG1_SPACE(buf) \
    ( (buf)->alloc - ((buf)->length + (buf)->start_offset))

/** Beginning of the buffer */
#define MYBUF_CONTIG1_HEAD(buf) \
    ( (buf)->data + (buf)->start_offset)

/** How much effective space would be saved via 'chop' */
#define MYBUF_CONTIG1_MAXSPACE(buf) \
    MYBUF_CONTIG1_SPACE(buf) + (buf)->start_offset

void mybuf_contig1_init(mybuf_contig1_t *buf);
void mybuf_contig1_cleanup(mybuf_contig1_t *buf);

void mybuf_contig1_append(mybuf_contig1_t *buf,
                   const void *data, unsigned long ndata);
void mybuf_contig1_compact(mybuf_contig1_t *buf);
void mybuf_contig1_chop(mybuf_contig1_t *buf, unsigned long offset);

#ifdef __cplusplus
}
#endif
#endif /* MYBUF_H */
