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
#define LISTLIMIT 16

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX(x,y) ((x) > (y) ? (x) : (y)) //  최댓값 구하기

#define PACK(size, alloc) ((size) | (alloc)) // 전체 페이지 정보

// 헤더의 정보를 읽어오거나 쓸 때 필요한 함수
#define GET(p)  (*(unsigned int *)(p)) // 헤더/푸터 주소값 반환
#define PUT(p,val)  (*(unsigned int *)(p) = (val))// 헤더/푸터 주소값 넣기

#define GET_SIZE(p) (GET(p) & ~0x7) // 헤더 전체 크기에서 하위 3비트를 제거한 값
#define GET_ALLOC(p)    (GET(p) & 0x1) // 헤더 하위 3비트의 할당 여부 확인(free/allocated)

#define HDRP(bp)    ((char *)(bp) - WSIZE) // 헤더 포인터 리턴
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 풋터 포인터 리턴

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE)) // 다음 블록 포인터 리턴
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)) // 이전 블록 포인터 리턴

//next,prev 포인터 지정
#define PUT_NEXT_Addr(bp,address)    (*(void **)(bp)= address)
#define PUT_PREV_Addr(bp,address)    (*(void **)((char *)bp + WSIZE) = address) // 헤더/푸터 주소값 가져오기

//현재 블록의 주소 가져오기 
#define GET_NEXT_Addr(bp) (*(void **)(bp))
#define GET_PREV_Addr(bp) (*((void **)((char *)(bp) + WSIZE)))

static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);

// 포인터 주소를 담을 리스트 만들기
void *free_lists[LISTLIMIT];
void *heap_listp;

//루트 지정
void *root;

// asize 크기에 맞는 인덱스 봔한
int Block_size(size_t asize);
void front_root(void* bp);
void remove_free(void* bp);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    
	/*root 포인터 초기화*/
    for (int i=0; i<LISTLIMIT;i++){
        free_lists[i] = NULL;
    }
	
    // heap_list는 처음 unsigned 부분을 카리키고 있음    
    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* Epilogue header */
 
    // heap-listp 는 padding header 과 footer 사이를 가리키고 있음
    heap_listp += (2*WSIZE);

    // 현재 가리키고 있는 포인터로 사용
    root = NULL;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
            return -1;

    return 0;
}

// 현재 확장해야 하는 블록의 갯수
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

// 블럭간 연결 담당
static void *coalesce(void *bp)
{
    // free할 블록의 좌우를 확인해서
    // prev_alloc : 현재 블록 포인트의 앞 블록이 할당 됨/할당 안됨 상태
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    // next_alloc : 현재 블록 R포인트의 다음 블록이 할당 됨/할당 안됨 상태
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // 현재 블록의 사이즈
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { /* Case 1 */
        front_root(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_free(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
        front_root(bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        remove_free(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        front_root(bp);
    }
    
    else { /* Case 4 */
        remove_free(PREV_BLKP(bp));
        remove_free(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        front_root(bp);
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

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);

    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}

// 블럭 사이즈에 맞는 가용 가능 리스트 찾기
static void *find_fit(size_t asize){
/* First-fit search */
    void *bp;
    int temp = Block_size(asize);
    for (int i=temp; i<LISTLIMIT;i++){
        // 연결 리스트 
	    root = free_lists[i];
        for (bp = root; bp != NULL; bp = GET_NEXT_Addr(bp)) {
            if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
                return bp;
                }
            }

    }
    return NULL; /* No fit */
}

// 값 넣을 때,
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    // printf("csize : %d\n",csize);
    // printf("asize : %d\n\n",asize);
    //쪼개는게 가능
    if ((csize - asize) >= (2*DSIZE)) { 
        remove_free(bp);        
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        front_root(bp);
    }
    //쪼개는게 불가능
    else {
        remove_free(bp);
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }

}
// root 붙여주기
void front_root(void* bp){
    int temp = Block_size(GET_SIZE(HDRP(bp)));
	root = free_lists[temp];

    PUT_NEXT_Addr(bp,root);
    if (root!=NULL){
        PUT_PREV_Addr(root, bp);
    }
    free_lists[temp] = bp;
}

// free 블록 빼기
void remove_free(void* bp) {
    int temp = Block_size(GET_SIZE(HDRP(bp)));
    root = free_lists[temp];
k
    if (bp!=root){
        PUT_NEXT_Addr(GET_PREV_Addr(bp),GET_NEXT_Addr(bp));
        // 삭제 했는데 남은게 NULL 인 경우 
        if (GET_NEXT_Addr(bp)!=NULL)
            PUT_PREV_Addr(GET_NEXT_Addr(bp),GET_PREV_Addr(bp));
    
    }else{
        free_lists[temp] = GET_NEXT_Addr(bp);
    }
    
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
            
    copySize = GET_SIZE(HDRP(oldptr))-2 * WSIZE - SIZE_T_SIZE;
    // copySize = *(size_t *)((char *)GET_SIZE(oldptr) -2 *WSIZE - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, size);
    mm_free(oldptr);
    return newptr;
}

/*현재 블록의 범위 확인*/
int Block_size(size_t asize){
    int idx = 0;
    for (int i=4; i<LISTLIMIT;i++){
        if (asize < 1<<i){
            break;
        }
        idx++;
    }

    // while ((idx < LISTLIMIT - 1) && (asize > 1)) {
    //     asize >>= 1;
    //     idx++;
    // }
    return idx;
}