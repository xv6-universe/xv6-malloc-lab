#include "kernel/types.h"
#include <stddef.h>
#include "user/user.h"
#include "ummalloc.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
//#define REALLOC
//#define RM
//#define LXY
//#define DEBUG
//
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
//#define LAST

#define SIZE_T_SIZE (ALIGN(sizeof(uint)))
/*
 * assets from CS:APP, Chapter 9.9, Page 599
 */

char *heap_listp;
char *seg_listp;
char *align_listp;

#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

/* @structure of the block:
 *
 * 1. we use an 8-byte header to store the size of the block
 * and whether the block is allocated.
 *
 *  31       | 3  2 | 1  0 |
 *  ------------------------
 *  |  size  | null |   p  |
 *
 * 2. we use two 4-byte pointers to store prev/next free-node pointer.
 * 3. we store the payload in the middle of the block.
 * 4. we store the padding part to align the block.
 * 5. we use an 4-byte footer to store the size of the block
 *
 * - bp is pointed at the beginning of #2
 */

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
/* free list storage */
#define PREV_FREE(bp) ((char *)(bp))
#define NEXT_FREE(bp) ((char *)(bp + WSIZE))



int mm_init(void);

void *mm_malloc(uint size);

void mm_free(void *ptr);

void *mm_realloc(void *ptr, uint size);

void *find_fit(size_t asize);

void place(void *bp, size_t asize, int exist);

void *coalesce(void *bp, int realloc, int target_size);

void *extend_heap(size_t words);

char *fit_list(size_t asize);

void remove_node(char *bp);

void insert_node(char *bp);

void put_old_node(char *bp, size_t size, int alloc);

void put_new_node(char *bp, size_t size, int alloc);

size_t align(size_t size);

/*
 * @brief initialize the malloc package.
 * we use segregated free-list to manage the free blocks.
 * the block sizes are from 2^3 to 2^14 and beyond
 *
 * we store the head of each free-list in the heap beginning
 */
int mm_init(void) {
  if ((heap_listp = sbrk(17 * WSIZE)) == (void *) -1)
    return -1;

  for (int i = 3; i <= 15; i++) {
    PUT(heap_listp + ((i - 3) * WSIZE), 0);
  }

  PUT(heap_listp + (13 * WSIZE), 0);              /* Alignment padding */
  PUT(heap_listp + (14 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
  PUT(heap_listp + (15 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
  PUT(heap_listp + (16 * WSIZE), PACK(0, 1)); /* Epilogue header */

  seg_listp = heap_listp;
  heap_listp += (15 * WSIZE);
  align_listp = heap_listp - (2 * WSIZE);

#ifdef DEBUG
  printf("heap_listp: %p\n", heap_listp);
  printf("seg_listp: %p\n", seg_listp);
  printf("align_listp: %p\n", align_listp);
#endif

  if (extend_heap(CHUNKSIZE / WSIZE) == 0)
    return -1;

  return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(uint size) {
#ifdef REALLOC
  printf("mm_malloc: %d\n", size);
#endif
  size_t asize = align(size); /* Adjusted block size */
  size_t extendsize; /* Amount to extend heap if no fit */
  char *bp;

  if (size == 0)
    return 0;

  if ((bp = find_fit(asize)) != 0) {
#ifdef REALLOC
    printf("find fit: %p\n", bp);
#endif
    place(bp, asize, 1);
    return bp;
  } else {
    extendsize = asize;
    if ((bp = extend_heap(extendsize / WSIZE)) == 0)
      return 0;
#ifdef REALLOC
    printf("find fit: %p\n", bp);
#endif
    place(bp, asize, 1);
    return bp;
  }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
#ifdef DEBUG
  printf("mm_free: %p\n", ptr);
#endif
  size_t size = GET_SIZE(HDRP(ptr));

  put_new_node(ptr, size, 0);
  coalesce(ptr, 0, 0);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, uint size) {
#ifdef REALLOC
  printf("mm_realloc: %p, size: %d\n", ptr, size);
#endif
  void *newptr = 0;

  if (ptr == 0) {
    return mm_malloc(size);
  } else if (size == 0) {
    mm_free(ptr);
    return 0;
  } else {
    size_t origin_size = GET_SIZE(HDRP(ptr));
    size_t asize = align(size);

    if (asize == origin_size) {
      return ptr;
    } else if (asize < origin_size) { // same as placing a node here
      // but we can't remove bp as it doesn't exist in linklist
      place(ptr, asize, 0);
      return ptr;
    } else {
      void *new_bp = coalesce(ptr, 1, asize);
      if (GET_SIZE(HDRP(new_bp)) >= asize) {
        if (new_bp != ptr) {
          memcpy(new_bp, ptr, size);
          place(new_bp, asize, 0);
        } else {
          place(new_bp, asize, 0);
        }
        return new_bp;
      } else {
        // we didn't merge block as it doesn't help
        newptr = mm_malloc(size);
        if (newptr == 0) {
          return 0;
        }
        memcpy(newptr, ptr, size);
        mm_free(ptr);
        return newptr;
      }
    }
  }
}


void *find_fit(size_t asize) {
#ifdef LXY
  printf("fit_list: %p\n", fit_list(asize));
#endif
  // we have to repeatedly try from the first available to the end
  for (char *bp = fit_list(asize); bp != align_listp; bp += WSIZE) {
#ifdef REALLOC
    printf("trying fit bp: %p\n", bp);
#endif
    char *node = (char *) ((uint64)GET(bp));
    while (node != 0) {
#ifdef REALLOC
      printf("node: %p\n", node);
      printf("node size: %d\n", GET_SIZE(HDRP(node)));
#endif
      if (asize <= GET_SIZE(HDRP(node))) {
        return node;
      } else {
        node = (char *) (uint64)GET(NEXT_FREE(node));
      }
    }
  }

  return 0;
}

void place(void *bp, size_t asize, int exist) {
#ifdef DEBUG
  printf("place: %p, size: %d\n", bp, asize);
#endif
  size_t csize = GET_SIZE(HDRP(bp));
  if (exist) {
    remove_node(bp);
  }

  if ((csize - asize) >= (2 * DSIZE)) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);

    // the split node's size should be judged
    int new_size = csize - asize;

    put_new_node(bp, new_size, 0);
    coalesce(bp, 0, 0);
  } else {
    put_old_node(bp, csize, 1);
  }
}

void *coalesce(void *bp, int realloc, int target_size) {
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
#ifdef REALLOC
  printf("bp: %p\n", bp);
  printf("prev_alloc: %d, next_alloc: %d\n", prev_alloc, next_alloc);
  printf("prev_block: %p, next_block: %p\n", PREV_BLKP(bp), NEXT_BLKP(bp));
#endif
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc) {
    if (!realloc) {
      insert_node(bp);
    }
    return bp;
  } else if (prev_alloc) {
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    if (!realloc || size >= target_size) {
      remove_node(NEXT_BLKP(bp));
      PUT(HDRP(bp), PACK(size, realloc));
      PUT(FTRP(bp), PACK(size, realloc));
    }
  } else if (next_alloc) {
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    if (!realloc || size >= target_size) {
      remove_node(PREV_BLKP(bp));
      PUT(FTRP(bp), PACK(size, realloc));
      PUT(HDRP(PREV_BLKP(bp)), PACK(size, realloc));
      bp = PREV_BLKP(bp);
    }
  } else {
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    if (!realloc || size >= target_size) {
      remove_node(PREV_BLKP(bp));
      remove_node(NEXT_BLKP(bp));
      PUT(HDRP(PREV_BLKP(bp)), PACK(size, realloc));
      PUT(FTRP(NEXT_BLKP(bp)), PACK(size, realloc));
      bp = PREV_BLKP(bp);
    }
  }

  if (!realloc) {
    insert_node(bp);
  }

  return bp;
}

void *extend_heap(size_t words) {
#ifdef LXY
  printf("extend_heap: %d\n", words);
#endif
#ifdef DEBUG
  printf("extend_heap: %d\n", words);
#endif
  char *bp;
  size_t size;

  size = words * WSIZE;
  if ((long) (bp = sbrk(size)) == -1) {
#ifdef REALLOC
    printf("sbrk failed\n");
#endif
    return 0;
  }
#ifdef DEBUG
  printf("extend_heap: %p\n", bp);
#endif
  put_new_node(bp, size, 0);
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

  return coalesce(bp, 0, 0);
}

char *fit_list(size_t asize) {
#ifdef DEBUG
  if (asize <= 8) {
    printf("fit size: 8\n");
  } else if (asize <= 16) {
    printf("fit size: 16\n");
  } else if (asize <= 32) {
    printf("fit size: 32\n");
  } else if (asize <= 64) {
    printf("fit size: 64\n");
  } else if (asize <= 128) {
    printf("fit size: 128\n");
  } else if (asize <= 256) {
    printf("fit size: 256\n");
  } else if (asize <= 512) {
    printf("fit size: 512\n");
  } else if (asize <= 1024) {
    printf("fit size: 1024\n");
  } else if (asize <= 2048) {
    printf("fit size: 2048\n");
  } else if (asize <= 4096) {
    printf("fit size: 4096\n");
  } else if (asize <= 8192) {
    printf("fit size: 8192\n");
  } else if (asize <= 16384) {
    printf("fit size: 16384\n");
  } else {
    printf("fit size: 32768\n");
  }
#endif

  if (asize <= 8) {
    return seg_listp;
  } else if (asize <= 16) {
    return seg_listp + WSIZE;
  } else if (asize <= 32) {
    return seg_listp + 2 * WSIZE;
  } else if (asize <= 72) {
    return seg_listp + 3 * WSIZE;
  } else if (asize <= 136) {
    return seg_listp + 4 * WSIZE;
  } else if (asize <= 264) {
    return seg_listp + 5 * WSIZE;
  } else if (asize <= 520) {
    return seg_listp + 6 * WSIZE;
  } else if (asize <= 1032) {
    return seg_listp + 7 * WSIZE;
  } else if (asize <= 2056) {
    return seg_listp + 8 * WSIZE;
  } else if (asize <= 4104) {
    return seg_listp + 9 * WSIZE;
  } else if (asize <= 8200) {
    return seg_listp + 10 * WSIZE;
  } else if (asize <= 16392) {
    return seg_listp + 11 * WSIZE;
  } else {
    return seg_listp + 12 * WSIZE;
  }
}

void remove_node(char *bp) {
  char *first_node = fit_list(GET_SIZE(HDRP(bp)));
  char *prev_bp = (char *) (uint64)GET(PREV_FREE(bp));
  char *next_bp = (char *) (uint64)GET(NEXT_FREE(bp));

#ifdef RM
  printf("$rm: %p\n", bp);
  printf("first_node: %p\n", first_node);
  printf("prev_bp: %p\n", prev_bp);
  printf("next_bp: %p\n", next_bp);
#endif

  if (prev_bp != 0) {
    PUT(NEXT_FREE(prev_bp), (uint64)(next_bp));
    if (next_bp != 0) {
      PUT(PREV_FREE(next_bp), (uint64)(prev_bp));
    }
  } else if (next_bp != 0) {
    PUT(first_node, (uint64)(next_bp));
    PUT(PREV_FREE(next_bp), 0);
  } else {
    PUT(first_node, 0);
  }

#ifdef DEBUG
  printf("@return from remove_node\n");
#endif
}

void insert_node(char *bp) {
  char *first_addr = fit_list(GET_SIZE(HDRP(bp)));
  char *next_node = (char *) (uint64)GET(first_addr);
  // refactor: due to size constraint, we need to sort!
  // first_addr denotes the prev node, while next_node denotes the next node

  for (; next_node != 0; next_node = (char *) (uint64)GET(NEXT_FREE(next_node))) {
#ifdef LAST
    printf("-------------------------------\n");
    printf("challenge size: %d\n", GET_SIZE(HDRP(bp)));
    printf("another size: %d\n", GET_SIZE(HDRP(next_node)));
#endif
    if (GET_SIZE(HDRP(next_node)) >= GET_SIZE(HDRP(bp))) {
      break;
    } else {
      first_addr = next_node;
    }
  }

#ifdef LAST
  printf("-------------------------------\n");
  printf("************insert_node************\n");
  printf("bp: %p\n", bp);
  printf("size: %d\n", GET_SIZE(HDRP(bp)));
  printf("first_addr: %p\n", first_addr);
  printf("next_node: %p\n", next_node);
  printf("-------------------------------\n");
#endif
  if (first_addr != fit_list(GET_SIZE(HDRP(bp))) && first_addr != 0) {
    if (next_node != 0) {
#ifdef LXY
      printf("with some head\n");
#endif
      PUT(PREV_FREE(next_node), (uint64) bp);
      PUT(NEXT_FREE(bp), (uint64) next_node);
      PUT(PREV_FREE(bp), (uint64) first_addr);
      PUT(NEXT_FREE(first_addr), (uint64) bp);
    } else {
#ifdef LXY
      printf("without head\n");
#endif
      PUT(NEXT_FREE(bp), 0);
      PUT(PREV_FREE(bp), (uint64) first_addr);
      PUT(NEXT_FREE(first_addr), (uint64) bp);
    }
  } else {
    if (next_node != 0) {
#ifdef LXY
      printf("with some head\n");
#endif

      PUT(PREV_FREE(next_node), (uint64) bp);
      PUT(NEXT_FREE(bp), (uint64) next_node);
      PUT(PREV_FREE(bp), 0);
      PUT(first_addr, (uint64) bp);
    } else {
#ifdef LXY
      printf("without head\n");
#endif
      PUT(first_addr, (uint64) bp);
      PUT(NEXT_FREE(bp), 0);
      PUT(PREV_FREE(bp), 0);
    }
  }
}

void put_old_node(char *bp, size_t size, int alloc) {
  PUT(HDRP(bp), PACK(size, alloc));
  PUT(FTRP(bp), PACK(size, alloc));
}

void put_new_node(char *bp, size_t size, int alloc) {
  PUT(HDRP(bp), PACK(size, alloc));
  PUT(FTRP(bp), PACK(size, alloc));
  PUT(NEXT_FREE(bp), 0);
  PUT(PREV_FREE(bp), 0);
}

size_t align(size_t size) {
  if (size <= DSIZE) {
    return 2 * DSIZE;
  } else {
    return DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }
}
