/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */
#define MINBLOCKSIZE 16

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc)) /* Pack a size and allocated bit into a word */

#define GET(p) (*(unsigned int *)(p)) /* read a word at address p */
#define PUT(p, val) (*(unsigned int *)(p) = (val)) /* write a word at address p */

#define GET_SIZE(p) (GET(p) & ~0x7) /* read the size field from address p */
#define GET_ALLOC(p) (GET(p) & 0x1) /* read the alloc field from address p */

#define HDRP(bp) ((char*)(bp) - WSIZE) /* given block ptr bp, compute address of its header */
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) /* given block ptr bp, compute address of its footer */

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp))) /* given block ptr bp, compute address of next blocks */
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp)-DSIZE)) /* given block ptr bp, compute address of prev blocks */

static char* heap_list;
static char* pre_listp;
static void* extend_heap(size_t words);
static void* coalesce(void *bp);
static void* next_fit(size_t asize);
static void place(void* bp, size_t asize);
int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);

static void* extend_heap(size_t words){
    char* bp;
    size_t size;
    
    size=(words%2)?(words+1)*WSIZE:words*WSIZE;
    if((long)(bp=mem_sbrk(size))==-1)
    	return NULL;
	
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));

    return coalesce(bp);
}


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if((heap_list=mem_sbrk(4*WSIZE))==(void*)-1){
    	return -1;
    }
    PUT(heap_list,0);

    PUT(heap_list + (1*WSIZE), PACK(DSIZE, 1));     /* 填充序言块 */
    PUT(heap_list + (2*WSIZE), PACK(DSIZE, 1));     /* 填充序言块 */
    PUT(heap_list + (3*WSIZE), PACK(0, 1));         /* 结尾块 */

    heap_list += (2*WSIZE);
    pre_listp=heap_list;
    /* 扩展空闲空间 */
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
     size_t asize; // adjusted block size
    size_t extendsize; // amount to extend heap if no fit
    char* bp;

    if (size == 0)
        return NULL;

    // adjusted block size to include overhead and alignment reqs
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = next_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    // no fit found, get more memory and place the block
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if(ptr==0)
        return;
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    size = GET_SIZE(HDRP(oldptr));
    copySize = GET_SIZE(HDRP(newptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize-WSIZE);
    mm_free(oldptr);
    return newptr;
}

void place(void *bp,size_t asize){
	size_t csize = GET_SIZE(HDRP(bp));
	
	if((csize-asize)>2*DSIZE){
		PUT(HDRP(bp), PACK(asize, 1));
        	PUT(FTRP(bp), PACK(asize, 1));
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize - asize, 0));
		PUT(FTRP(bp), PACK(csize - asize, 0));
	}
	else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        }
}


void* coalesce(void *bp){
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));     /* 前一块大小 */
    	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));     /* 后一块大小 */
    	size_t size = GET_SIZE(HDRP(bp));                       /* 当前块大小 */

    /*
     * 四种情况：前后都不空, 前不空后空, 前空后不空, 前后都空
     */
    /* 前后都不空 */
    	if(prev_alloc && next_alloc){
        	return bp;
    	}
    /* 前不空后空 */
    	else if(prev_alloc && !next_alloc){
        	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));  //增加当前块大小
        	PUT(HDRP(bp), PACK(size, 0));           //先修改头
        	PUT(FTRP(bp), PACK(size, 0));           //根据头部中的大小来定位尾部
    	}
    /* 前空后不空 */
    	else if(!prev_alloc && next_alloc){
        	size += GET_SIZE(HDRP(PREV_BLKP(bp)));  //增加当前块大小
        	PUT(FTRP(bp), PACK(size, 0));
        	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        	bp = PREV_BLKP(bp);                     //注意指针要变
    	}
    /* 都空 */
    	else{
        	size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));  //增加当前块大小
        	PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        	bp = PREV_BLKP(bp);
    	}
    	return bp;
}

static void* next_fit(size_t asize)
{
    for (char* bp = pre_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize)
        {
            pre_listp = bp;
            return bp;
        }
    }

    for (char* bp = heap_list; bp != pre_listp; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize)
        {
            pre_listp = bp;
            return bp;
        }
    }
    return NULL;
}








