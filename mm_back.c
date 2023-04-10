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
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7) // 사이즈에 맞는 블럭의 개수를 부여

// CHUNKSIZE
#define CHUNKSIZE (1<<12) 
#define WSIZE   4
#define DSIZE   8

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX(x,y) ((x) > (y) ? (x) : (y)) //  최댓값 구하기

#define PACK(size, alloc) ((size) | (alloc)) // 전체 페이지 정보

// 헤더의 정보를 읽어오거나 쓸 때 필요한 함수
#define GET(p)  (*(unsigned int *)(p)) // 헤더/푸터 주소값 반환
#define PUT(p,val)  (*(unsigned int *)(p) = (val))// 헤더/푸터 주소값 가져오기

#define GET_SIZE(p) (GET(p) & ~0x7) // 헤더 전체 크기에서 하위 3비트를 제거한 값
#define GET_ALLOC(p)    (GET(p) & 0x1) // 헤더 하위 3비트의 할당 여부 확인(free/allocated)

#define HDRP(bp)    ((char *)(bp) - WSIZE) // 헤더 포인터 리턴
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 풋터 포인터 리턴

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE)) // 다음 블록 포인터 리턴
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)) // 이전 블록 포인터 리턴

#define PUT_NEXT_ADDR(bp,val) (*(void **)((char *)bp) = val) // NEXT에 주소 값 넣기
#define PUT_PREV_ADDR(bp,val) (*(void **)((char *)bp + WSIZE) = val) // PREV에 주소 값 넣기

#define GET_NEXT_ADDR(bp) (*(void **)((char *)bp)) // NEXT의 주소 값 출력
#define GET_PREV_ADDR(bp) (*(void **)((char *)bp + WSIZE)) // PREV의 주소 값 출력

static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);

void remove_free(void* bp); /* free된 블럭 가용 리스트에서 제거*/
void add_free(void* bp); /*root에 블럭 추가*/

void *heap_listp;  /*heap의 첫번째 위치*/
void *free_listp; /*현재 root의 위치*/

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */ 
    // 6 WSIZE 할당
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;
        
    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), NULL); /* NEXT */
    PUT(heap_listp + (3*WSIZE), NULL); /* PREV */
    PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (5*WSIZE), PACK(0, 1)); /* Epilogue header */
    
    free_listp = heap_listp + (2*WSIZE); /* LIFO의 루트 값 */ 
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
            return -1;
    return 0;
}
// words : 블록의 갯수
static void *extend_heap(size_t words)
{
char *bp;
size_t size;

/* Allocate an even number of words to maintain alignment */
size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
if ((long)(bp = mem_sbrk(size)) == -1)
    return NULL;

/* Initialize free block header/footer and the epilogue header */
PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

/* Coalesce if the previous block was free */
return coalesce(bp); //앞을 검사해서 free가 있다면 확장된 값과 합친다.
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
size_t size = GET_SIZE(HDRP(bp));
PUT(HDRP(bp), PACK(size, 0));
PUT(FTRP(bp), PACK(size, 0));

coalesce(bp);
}

static void *coalesce(void *bp)
{
    // printf("hell\n");
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { /* Case 1 */
        // 새로운 블록 bp root와 연결
        add_free(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        // 기존 블록에 연결되어 있던 연결 끊기
        remove_free(bp);
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
        // 새로운 블록 bp root와 연결
        add_free(bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        // 기존 블록 연결 끊기
        remove_free(PREV_BLKP(bp));
        // 기존 블록 할당 완료 후 root와 연결
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        add_free(bp);
    }
    
    else { /* Case 4 */
        // 기존 블록 앞 뒤 연결 
        remove_free(PREV_BLKP(bp));
        remove_free(NEXT_BLKP(bp));
        // root와 연결
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        add_free(bp);
    }

    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    // printf("max : %u\n", mem_heap_hi());
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);

    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    // printf("bp: %u\n\n", bp);
    place(bp, asize);

    return bp;
}
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    
    // if (p == (void *)-1)
	//     return NULL;
    
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }

// 요구된 블록 사이즈에 맞는 블록 찾기 => 없다면 확장
static void *find_fit(size_t asize){
/* First-fit search */
    void *bp;
    // printf("find: %d\n",asize);
    for (bp = GET_NEXT_ADDR(heap_listp); bp!=NULL; bp = GET_NEXT_ADDR(bp)) {
        // printf("bp: %u\n", bp);
        // printf("size: %d\n", GET_SIZE(HDRP(bp)));
        // printf("alloc: %d\n\n", GET_ALLOC(HDRP(bp)));
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
            }
        }
    return NULL; /* No fit */
}

//
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    // printf("csize : %d\n",csize);
    // printf("asize : %d\n\n",asize);
    // 쪼갤 필요 0
    remove_free(bp);
    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        add_free(bp);
        
    }
    //쪼갤 필요 X
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

void remove_free(void* bp){
    // 처음꺼를 없앨 때,
    if (free_listp==bp){
        PUT_PREV_ADDR(GET_NEXT_ADDR(bp),NULL);
        free_listp = GET_NEXT_ADDR(bp);
    }
    else {
        // 프리 리스트 사이 값을 없앨 때,
        PUT_NEXT_ADDR(GET_PREV_ADDR(bp),GET_NEXT_ADDR(bp));
        PUT_PREV_ADDR(GET_NEXT_ADDR(bp),GET_PREV_ADDR(bp));
    }
}

void add_free(void* bp){
    //root에 연결되는 경우
    PUT_NEXT_ADDR(bp,free_listp);
    PUT_PREV_ADDR(free_listp,bp);
    PUT_PREV_ADDR(bp,NULL);
    free_listp = bp;
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
    
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    // copySize = *(size_t *)((char *)GET_SIZE(oldptr) -2 *WSIZE - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, size);
    mm_free(oldptr);
    return newptr;
}