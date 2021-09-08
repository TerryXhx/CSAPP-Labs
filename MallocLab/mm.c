/* 
 * Simple, 32-bit and 64-bit clean allocator.
 * Based on explicit segregated free lists.
 * The size of each block of each free list is incremental.
 * Placement strategy: Find the smallest free block with size larger than the requirement size.
 * Blocks must be aligned to doubleword (8 byte) boundaries. 
 * Minimum block size is 16 bytes.
 * 
 * Block layout: 
 *      Allocated Block:
 *          [Header(4 Bytes): <size><001>]
 *          [Paylaod and alignment]
 *          [Footer(4 Bytes): <size><001>]
 *      Free Block:
 *          [Header(4 Bytes): <size><000>]
 *          [PRED(4 Bytes): relative address to heap_listp of its predecessor on its free list]
 *          [SUCC(4 Bytes): relative address to heap_listp of its successor on its free list]
 *          [...]
 *          [Footer(4 Bytes): <size><000>]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memlib.h"
#include "mm.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* Basic constants and macros */
#define WSIZE     4         /* Word and header/footer size (bytes) */
#define DSIZE     8         /* Double word size (bytes) */
#define CLASS_CNT 14        /* Class count */
#define CHUNKSIZE (1 << 8) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)        (*(unsigned int *)(p))
#define PUT(p, val)   (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)   (GET(p) & ~0x7)
#define GET_ALLOC(p)  (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)      ((char *)(bp) - WSIZE)
#define FTRP(bp)      ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Convert addresses between relative and absolute addresses */
#define ABS2REL(abs_bp) ((unsigned int)((abs_bp) ? (unsigned long)((abs_bp) - (long)heap_listp) : 0))
#define REL2ABS(rel_bp) ((char *)((rel_bp) ? ((char *)heap_listp + rel_bp) : NULL)) 

/* Given block ptr bp, compute the address of its predecessor and successor fields */
#define PREDP(bp) ((char *)(bp))
#define SUCCP(bp) ((char *)(bp) + WSIZE)

/* Given block ptr, compute the address of its predecessor and successor block */
#define GET_PRED(bp) (REL2ABS(GET(PREDP(bp))))
#define GET_SUCC(bp) (REL2ABS(GET(SUCCP(bp))))

/* Given head ptr, get/set its value */
#define GET_HEAD(headp)      (REL2ABS(GET(headp)))
#define SET_HEAD(headp, next) (PUT(headp, ABS2REL(next)))

/* Given block ptr and a value, set its predecessor/successor filed to the value */
#define SET_PRED(bp, pred) (PUT(PREDP(bp), ABS2REL(pred)))
#define SET_SUCC(bp, succ) (PUT(SUCCP(bp), ABS2REL(succ)))

/* Private global variables */
static char *heap_listp = NULL; /* Points to first block */

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *place(void *bp, size_t asize);
static void insert_to_free_list(char *bp);
static void remove_from_free_list(char *bp);
static char *get_listp(size_t size);
static void printblock(char *bp);

/*
 * mm_init 
 *  - Called when a new trace starts.
 */
int mm_init(void) {
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk((CLASS_CNT + 4) * WSIZE)) == (void *)(- 1))
        return -1;
    
    /* Put the head pointer of 14 classses at the beginning of the heap */
    for (size_t class_id = 0; class_id < CLASS_CNT; ++class_id)
        SET_HEAD(heap_listp + class_id * WSIZE, NULL);
    heap_listp += CLASS_CNT * WSIZE;

    PUT(heap_listp, 0);                            /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2 * WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * malloc 
 *  - Allocate a block by incrementing the brk pointer.
 *  - Always allocate a block whose size is a multiple of the alignment.
 */
void *malloc(size_t size) {
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if not fit */
    char *bp;

    /* Ignore spurious requesets */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * free 
 *  - Free a block.
 */
void free(void *bp) {
    if (bp == NULL)
        return;

    size_t size = GET_SIZE(HDRP(bp));
    if (heap_listp == NULL)
        mm_init();

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    SET_PRED(bp, NULL);
    SET_SUCC(bp, NULL);

    insert_to_free_list(bp);
    coalesce(bp);
}

/*
 * realloc 
 *  - Change the size of the block by mallocing a new block,
 *  - copying its data, and freeing the old block.
 */
void *realloc(void *oldptr, size_t size) {
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        free(oldptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(oldptr == NULL)
        return malloc(size);

    newptr = malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr)
        return 0;

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(oldptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    /* Free the old block. */
    free(oldptr);

    return newptr;
}

/*
 * calloc 
 *  - Allocate the block and set it to zero.
 */
void *calloc (size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    void *newptr;

    newptr = malloc(bytes);
    memset(newptr, 0, bytes);

    return newptr;
}

/*
 * mm_checkheap 
 *  - Check if the heap is safe.
 */
void mm_checkheap(int line) {
    printf("---------- Enter checkheap ----------\n");
    printf("Line:\t%d\n", line);
    printf("heap_listp:\t%p\n", heap_listp);

    printf("********** Entire Heap Blocks **********\n");
    char *bp = heap_listp;
    int block_id = 0;
    while (GET_SIZE(HDRP(bp))) {
        printf("Block ID:\t%d\n", block_id++);
        printblock(bp);
        bp = NEXT_BLKP(bp);
    }
    printf("********** End Entire Heap Blocks **********\n\n");


    printf("********** Segragated Lists **********\n");
    for (int i = 0; i < CLASS_CNT; ++i) {
        printf("Class:\t%d\n", i);
        int empty_cnt = 0;
        char *head = GET_HEAD(heap_listp + (i - CLASS_CNT - 2) * WSIZE);
        if (!head) {
            printf("No empty block\n");
            continue;
        } else {
            printf("Empty block ID: %d\n", empty_cnt++);
            printblock(head);
        }
        char *bp = GET_SUCC(bp);
        while (bp) {
            printf("Empty block ID: %d\n", empty_cnt++);
            printblock(bp);
            bp = GET_SUCC(bp);
        }
    }
    printf("********** End Segerated Lists **********\n\n");

    printf("---------- Quit checkheap ----------\n\n");
}

/* The remaining routines are internal helper routines */

/* 
 * extend_heap 
 *  - Extend heap with free block.
 *  - Return the block pointer.
 */
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* Allocate an even number of words to maitain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    SET_PRED(bp, NULL);                   /* PRED field */
    SET_SUCC(bp, NULL);                   /* SUCC field */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Insert the new free block to its free list */
    insert_to_free_list(bp);

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/* 
 * coalesce 
 *  - Coalesce two neighbouring block if possible.
 *  - Return the ptr to the coalesced block.
 */
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    char *prev_bp = PREV_BLKP(bp);
    char *next_bp = NEXT_BLKP(bp);

    if (prev_alloc && next_alloc)           /* Case 1 */
        return bp;

    else if (prev_alloc && !next_alloc) {   /* Case 2 */
        remove_from_free_list(bp);
        remove_from_free_list(next_bp);

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

        insert_to_free_list(bp);
    }
                                    
    else if (!prev_alloc && next_alloc) {   /* Case 3 */
        remove_from_free_list(bp);
        remove_from_free_list(prev_bp);

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);

        insert_to_free_list(bp);
    }

    else {                                  /* Case 4 */
        remove_from_free_list(prev_bp);
        remove_from_free_list(bp);
        remove_from_free_list(next_bp);

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);

        insert_to_free_list(bp);
    }

    return bp;
}

/*
 * find_fit 
 *  - Find the fit ptr for the block with asize bytes.
 */
static void *find_fit(size_t asize) {
    char *headp = get_listp(asize);
    char *list_headp_end = heap_listp - (2 * WSIZE);

    while (headp != list_headp_end) {
        char *bp = GET_HEAD(headp);
        while (bp) {
            if (GET_SIZE(HDRP(bp)) >= asize)
                return bp;
            bp = GET_SUCC(bp);
        }
        headp += WSIZE;
    }

    return NULL;
}

/* 
 * place 
 *  - Set the block at ptr allocated and split if the remainder size is large enough.
 */
static void *place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    
    remove_from_free_list(bp);

    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        char *free_bp = NEXT_BLKP(bp);
        PUT(HDRP(free_bp), PACK(csize - asize, 0));
        PUT(FTRP(free_bp), PACK(csize - asize, 0));

        insert_to_free_list(free_bp);
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }

    return bp;
}

/* 
 * insert_to_free_list
 *  - Insert the block to the free list that it should be inserted into.
 */
static void insert_to_free_list(char *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    char *headp = get_listp(size);
    char *pred = headp, *succ = GET_HEAD(headp);

    /* Find the succ block with smallest size which is larger than given block size */
    while (succ && GET_SIZE(HDRP(succ)) < size) {
        pred = succ;
        succ = GET_SUCC(succ);
    }

    /* Case 1: headp(pred) -> bp -> NULL(succ) */
    if (pred == headp && !succ) {
        SET_HEAD(headp, bp);

        SET_PRED(bp, NULL);
        SET_SUCC(bp, NULL);
    }

    /* Case 2: headp(pred) -> bp -> succ */
    else if (pred == headp && succ) {
        SET_HEAD(headp, bp);
        
        SET_PRED(bp, NULL);
        SET_SUCC(bp, succ);

        SET_PRED(succ, bp);
    }

    /* Case 3: pred -> bp -> NULL(succ) */
    else if (pred != headp && !succ) {
        SET_SUCC(pred, bp);

        SET_PRED(bp, pred);
        SET_SUCC(bp, succ);
    }

    /* Case 4: pred -> bp -> succ */
    else if (pred != headp && succ) {
        SET_SUCC(pred, bp);

        SET_PRED(bp, pred);
        SET_SUCC(bp, succ);

        SET_PRED(succ, bp);
    }
}

/* 
 * remove_from_free_list
 *  - Remove the block from the free list.
 */
static void remove_from_free_list(char *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    char *headp = get_listp(size); 
    char *pred = GET_PRED(bp), *succ = GET_SUCC(bp);

    /* Case1: head(pred) -> bp -> NULL(succ)  */
    if (pred == NULL && succ == NULL) 
        SET_HEAD(headp, NULL);

    /* Case 2: head(pred) -> bp => succ */
    else if (pred == NULL && succ != NULL) {
        SET_HEAD(headp, succ);

        SET_PRED(succ, NULL);
    }

    /* Case 3: pred -> bp -> NULL(succ) */
    else if (pred != NULL && succ == NULL) 
        SET_SUCC(pred, NULL);

    /* Case 4: pred -> bp -> succ */
    else if (pred != NULL && succ != NULL) {
        SET_SUCC(pred, succ);

        SET_PRED(succ, pred);
    }
}

/* 
 * get_listp
 *  - Given the block size, get the header of list that it shouold be placed.
 */
static char *get_listp(size_t size) {
    int class_id;

    if (size <= 8)
        class_id = 0;
    else if (size <= 16)
        class_id = 1;
    else if (size <= 24)
        class_id = 2;
    else if (size <= 32)
        class_id = 3;
    else if (size <= 64)
        class_id = 4;
    else if (size <= 128)
        class_id = 5;
    else if (size <= 256)
        class_id = 6;
    else if (size <= 512)
        class_id = 7;
    else if (size <= 1024)
        class_id = 8;
    else if (size <= 2048)
        class_id = 9;
    else if (size <= 4096)
        class_id = 10;
    else if (size <= 8192)
        class_id = 11;
    else if (size <= 16384)
        class_id = 12;
    else
        class_id = 13;
    
    return heap_listp + (class_id - CLASS_CNT - 2) * WSIZE;
}


/*
 * printblock
 *  - Gicven block pointer bp, print the basic infomation of the block.
 */
static void printblock(char *bp) {
    printf("###############\n");
    printf("Address:\t%p\n", bp);

    int size = GET_SIZE(HDRP(bp));
    int get_alloc = GET_ALLOC(HDRP(bp));

    if (get_alloc) {
        printf("Allocated\n");
        printf("Size:\t%d\n", size);
    } else {
        printf("Free\n");
        char *succ = GET_SUCC(bp);
        printf("Size:\t%d\n", size);
        printf("SUCC address:\t%p\n", succ);
    }

    printf("###############\n\n");
}