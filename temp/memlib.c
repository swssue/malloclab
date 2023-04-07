#define WSIZE   4 # 워드 바이트
#define DSIZE   8 # 더블 워드 바이트
#define CHUNKSIZE (1<<12) # 초기 확장을 위한 크기

#define MAX(x,y) ((x) > (y) ? (x) : (y)) #  최댓값 구하기

#define PACK(size, alloc) ((size) | (alloc)) # 헤더와 풋터에 저장할 수 있는 값 반환


#define GET(p)  (*(unsigned int *)(p)) # p값 반환
#define PUT(p,val)  (*(unsigned int *)(p) = (val)) $ p 값 저장

#define GET_SIZE(p) (GET(p) & ~0x7) # 헤더 또는 풋터의 sise 리턴
#define GET_ALLOC(p)    (GET(p) & 0x1) # 할당 비트 리턴

#define HDRP(bp)    ((char *)(bp) - WSIZE) # 헤더 포인터 리턴
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) # 풋터 포인터 리턴

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) # 다음 블록 포인터 리턴
#define PREV_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - DSIZE))) # 이전 블록 포인터 리턴



