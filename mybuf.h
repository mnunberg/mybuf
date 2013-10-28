#ifndef MYBUF_H
#define MYBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"

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

#define MYBUF_CONTIG1_TAIL(buf) \
    ( (buf)->data + (buf)->start_offset + (buf)->length)

/** How much effective space would be saved via 'chop' */
#define MYBUF_CONTIG1_MAXSPACE(buf) \
    MYBUF_CONTIG1_SPACE(buf) + (buf)->start_offset

void mybuf_contig1_init(mybuf_contig1_t *buf);
void mybuf_contig1_cleanup(mybuf_contig1_t *buf);

void mybuf_contig1_append(mybuf_contig1_t *buf,
                   const void *data, unsigned long ndata);
void mybuf_contig1_compact(mybuf_contig1_t *buf);
void mybuf_contig1_chop(mybuf_contig1_t *buf, unsigned long offset);

typedef enum {
    /**
     * The underlying buffer is *not* mapped to a contig1 structure but has
     * rather been allocated directly via malloc().
     */
    MYBUF_REGION_F_ALLOCATED = 1 << 0,

    /**
     * This region has been pinned.
     */
    MYBUF_REGION_F_PINNED = 1 << 1,

    /**
     * Region structure is user-allocated, don't modify the structure itself
     */
    MYBUF_REGION_F_STRUCTUALLOC = 1 << 2,

    /**
     * Buffer contents have been flushed (and thus is no longer a member
     * of the send queue list)
     */
    MYBUF_REGION_F_FLUSHED = 1 << 3
} mybuf_region_flags_t;

/**
 * Structure representing a buffer region. A buffer region represents a mapping
 * within a given buffer, and contains a back pointer so that references may
 * be updated and/or relocated in case the master buffer may be resized.
 *
 * In a normal use case, a user of this buffer will request allocation as
 * normal, and instead of receiving a pointer, will receive a region. This
 * region may be dynamically updated (unless 'pin()' is called)
 */
struct mybuf_region_st;

typedef struct mybuf_region_st {
    unsigned char flags;

    /** Length of region */
    unsigned long length;

    /** Buffer containing the data */
    char *buf;

    /** Pointers to the next and previous regions within the order */
    lcb_list_t ll;
} mybuf_region_t;

/**
 * Next step in our buffer configuration:
 *
 * Various regions will be joined together mapped to a specific buffer; the
 * order of the regions does not necessarily correspond to the order of the
 * buffer and thus reading the buffer contiguously does not necessitate the
 * reading of the regions in order.
 *
 * However it is guaranteed that each region in itself shall be contiguous.
 *
 * A region may request a 'pin' which is an indicator that the underlying
 * memory is being consumed/used/read by a non-region aware subsystem, and that
 * the underlying contents of the buffer shall not be allocated, specifically
 * this means that routines like 'compact' and 'realloc' shall not be called.
 */
typedef struct {
    mybuf_region_t regions;
    mybuf_region_t flushed_regions;

    /** Requests to 'pin()' */
    int pinned;

    /** Used for maintaining the offset at which to flush the first region */
    unsigned long flush_offset;

    /** Underlying buffer structure */
    mybuf_contig1_t buf;
} mybuf_regpool_t;


#define MYBUF_IOV_MAX 16
typedef struct {
    void *iov_base;
    unsigned long iov_len;
} mybuf_generic_iov;


/**
 * Routines to initialize and cleanup a region pool
 */
void mybuf_regpool_init(mybuf_regpool_t *pool);
void mybuf_regpool_clean(mybuf_regpool_t *pool);

/**
 * Reserves a region of exactly 'size' bytes
 * @param pool [in] the pool to use
 * @param size [in] the number of bytes this region requires
 * @param region [in/out] a pointer to an allocated region, or NULL.
 *  If `*region` is not NULL, then it is assumed that the user has embedded the
 *  region structure somewhere and it will not be allocated, otherwise the
 *  region is allocated.
 *
 *  In both cases, regpool_free_region() shall be called when the region is
 *  no longer needed
 */
void mybuf_regpool_get_region(mybuf_regpool_t *pool,
                              unsigned long size,
                              mybuf_region_t **region);


/**
 * 'pins' this region to its pointer. When a region is pinned, it is guaranteed
 * that the underlying '->buf' pointer will not change (e.g. the contents of
 * the memory shall not be relocated on reallocation/compaction) until
 * regpool_unpin() is called
 */
void mybuf_regpool_pin(mybuf_regpool_t *pool, mybuf_region_t *region);

/**
 * Unpins this region. When a region has been unpinned, the memory contents
 * of the underlying buffer may change.
 */
void mybuf_regpool_unpin(mybuf_regpool_t *pool, mybuf_region_t *region);

/**
 * Releases this region from the pool. Depending on how the region itself
 * was allocated (see get_region) the structure itself may be freed as well.
 *
 * Note that the region *must* be unpinned before free_region is called.
 */
void mybuf_regpool_free_region(mybuf_regpool_t *pool,
                               mybuf_region_t *region);


/**
 * Get an IOV-like structure for outputting to the network buffers.
 * Call iov_done() with the number of bytes written when finished
 * @param pool the pool
 * @param iov an array of iov structures, up to IOV_MAX
 * @param niov how many elements in the array
 */
void mybuf_regpool_iov_get(mybuf_regpool_t *pool,
                           mybuf_generic_iov *iov,
                           unsigned int niov);

/**
 * Call when a certain number of bytes have been written to the network.
 * @param nused how many bytes were sent on the network
 */
void mybuf_regpool_iov_done(mybuf_regpool_t *pool, unsigned long nused);

#ifdef __cplusplus
}
#endif
#endif /* MYBUF_H */
