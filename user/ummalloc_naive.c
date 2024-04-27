#include "kernel/types.h"
#include <stddef.h>
//
#include "user/user.h"

//
#include "ummalloc.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
//
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(uint)))

static char *heap_listp;

#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(uint *)(p))
#define PUT(p, val) (*(uint *)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

int mm_init(void);
void *mm_malloc(uint size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, uint size);
void *find_fit(size_t asize);
void place(void *bp, size_t asize);
void *coalesce(void *bp);
void *extend_heap(size_t words);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
  if ((heap_listp = sbrk(4*WSIZE)) == (void *)-1)
    return -1;

  PUT(heap_listp, 0); /* Alignment padding */
  // printf("heap_listp: %p\n", heap_listp);
  PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
  // printf("heap_listp + (1*WSIZE): %p\n", heap_listp + (1*WSIZE));
  PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
  // printf("heap_listp + (2*WSIZE): %p\n", heap_listp + (2*WSIZE));
  PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* Epilogue header */
  // printf("heap_listp + (3*WSIZE): %p\n", heap_listp + (3*WSIZE));
  heap_listp += (2*WSIZE);

  if (extend_heap(CHUNKSIZE/WSIZE) == 0)
    return -1;
  return 0;
}

void *find_fit(size_t asize) {
  char *bp;

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
      return bp;
    }
  }
  return 0;
}

void place(void *bp, size_t asize) {
  // printf("place: %p, size: %d\n", bp, asize);
  size_t csize = GET_SIZE(HDRP(bp));

  if ((csize - asize) >= (2*DSIZE)) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(csize-asize, 0));
    PUT(FTRP(bp), PACK(csize-asize, 0));
  } else {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}

void *coalesce(void *bp) {
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc) {
    return bp;
  } else if (prev_alloc && !next_alloc) {
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  } else if (!prev_alloc && next_alloc) {
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  } else {
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  return bp;
}

void *extend_heap(size_t words) {
  // printf("extend_heap: %d\n", words);
  char *bp;
  size_t size;

  size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
  if ((long)(bp = sbrk(size)) == -1)
    return 0;

  // printf("extend_heap: %p\n", bp);

  PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
  PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

  return coalesce(bp);
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(uint size) {
  // printf("mm_malloc: %d\n", size);
  size_t asize; /* Adjusted block size */
  size_t extendsize; /* Amount to extend heap if no fit */
  char *bp;

  if (size == 0)
    return 0;

  if (size <= DSIZE)
    asize = 2*DSIZE;
  else
    asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

  if ((bp = find_fit(asize)) != 0) {
    place(bp, asize);
    return bp;
  }

  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize/WSIZE)) == 0)
    return 0;
  place(bp, asize);
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
  // printf("mm_free: %p\n", ptr);
  size_t size = GET_SIZE(HDRP(ptr));

  PUT(HDRP(ptr), PACK(size, 0));
  PUT(FTRP(ptr), PACK(size, 0));
  coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, uint size) {
  // printf("mm_realloc: %p, size: %d\n", ptr, size);
  size_t oldsize;
  void *newptr;

  if (size == 0) {
    mm_free(ptr);
    return 0;
  }

  if (ptr == 0) {
    return mm_malloc(size);
  }

  newptr = mm_malloc(size);

  if (!newptr) {
    return 0;
  }

  oldsize = GET_SIZE(HDRP(ptr));
  if (size < oldsize) {
    oldsize = size;
  }
  memcpy(newptr, ptr, oldsize);
  mm_free(ptr);
  return newptr;
}
