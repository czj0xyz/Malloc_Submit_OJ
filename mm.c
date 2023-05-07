/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  Blocks are never coalesced or reused.  The size of
 * a block is found at the first aligned word before the block (we need
 * it for realloc).
 *
 * This code is correct and blazingly fast, but very bad usage-wise since
 * it never frees anything.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define LOG_ALIGNMENT 3
#define SKIP_SIZE (LISTNUM<<2)
#define WSIZE 4
#define LISTSIZE 4
#define BARSIZE ((WSIZE<<1) + (LISTSIZE<<1))
#define LOG_LISTSIZE 2

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
// #define SIZE_PTR(p)  ((size_t*)(((char*)(p)) - SIZE_T_SIZE))

typedef unsigned int uint;
typedef unsigned char uchar;

#define GET_PTR_VAL(p) (*((uint*)(p)))
#define PUT_PTR_VAL(p,val) ((*((uint*)(p))) = (val))
#define ADD_PTR_VAL(p,val) ((*((uint*)(p))) |= (val))
#define DEL_PTR_VAL(p,val) ((*((uint*)(p))) &= (~(val)))

#define GET_BLOCK_SIZE(p)  ((GET_PTR_VAL(p) & ~0x7))
#define GET_BLOCK_STATE(p) ((GET_PTR_VAL(p) & 1))
#define GET_PRE_BLOCK_STATE(p) (GET_BLOCK_STATE(GET_PRE_BLOCK_PTR(p)))
#define GET_NEXT_BLOCK_STATE(p) (GET_BLOCK_STATE(GET_NEXT_BLOCK_PTR(p)))
#define GET_LISTPRE(p)  GET_TRUE_PTR(GET_PTR_VAL(GET_LISTPRE_PTR(p)))
#define GET_LISTNEXT(p) GET_TRUE_PTR(GET_PTR_VAL(GET_LISTNEXT_PTR(p)))
#define GET_PTR_UINT(p) ((uint)((size_t)(p) - (size_t)(mem_heap_lo())))
#define GET_TRUE_PTR(p) ((uint*)((size_t)(p) + (size_t)(mem_heap_lo())))

#define GET_NEXT_BLOCK_PTR(p) ((uint*)(((uchar*)(p)) + GET_BLOCK_SIZE(p)))
#define GET_PRE_BLOCK_PTR(p) ((uint*)( ((uchar*)(p)) - GET_BLOCK_SIZE((uchar*)(p) - WSIZE) ))
#define GET_FOOTER_PTR(p) ((uint*)((uchar*)(p) + GET_BLOCK_SIZE(p) - WSIZE))
// #define GET_RET_PTR(p) ((void*)((uchar*)(p) + WSIZE + (LISTSIZE<<1)))
// #define GET_HEAD_PTR(p) ((uint*)((uchar*)(p) - WSIZE - (LISTSIZE<<1)))
#define GET_RET_PTR(p) ((void*)((uchar*)(p) + WSIZE))
#define GET_HEAD_PTR(p) ((uint*)((uchar*)(p) - WSIZE))
#define GET_LISTPRE_PTR(p) ((uint*)((uchar*)(p) + WSIZE))
#define GET_LISTNEXT_PTR(p) ((uint*)((uchar*)(p) + (WSIZE<<1)))

#define END_BLOCK(p) (((long)GET_NEXT_BLOCK_PTR(p)) >= ((long)mem_heap_hi()))
#define HEAP_ST (mem_heap_lo() + SKIP_SIZE)

#define FREE_HEAD(k) ((uint*)((uchar*)(mem_heap_lo()) + (k<<LOG_LISTSIZE)))


#define LISTNUM 7
#define UPPER_BOUND(k) (((size_t)1)<<((k*3>>1)+1))
// #define UPPER_BOUND(k) (k<=30?k+2:((size_t)1)<<((k<<1)-55)) // LISTNUM 35
// #define UPPER_BOUND(k) (k<=14?k+2:((size_t)1)<<((k<<1)-24)) // LISTNUM 19

/*
 * mm_init - Called when a new trace starts.
 */
int mm_init(void){
	uchar *p = mem_sbrk(SKIP_SIZE);
	if((long)p < 0)return -1;
	for(int i=0;i<LISTNUM;i++)
		PUT_PTR_VAL(FREE_HEAD(i),0);
	return 0;
}

/*
 * malloc - Allocate a block by incrementing the brk pointer.
 *      Always allocate a block whose size is a multiple of the alignment.
 */
inline void insert_block(uint *now){
	/*
		now->nex = head->nex;
		head->nex->pre = now;
		now->pre = head;
		head->nex = now;
	*/
	size_t k = 0, sz = GET_BLOCK_SIZE(now)>>LOG_ALIGNMENT;
	for(k=0;k<LISTNUM-1&&sz>=UPPER_BOUND(k);k++);

	PUT_PTR_VAL(GET_LISTNEXT_PTR(now), GET_PTR_VAL(FREE_HEAD(k)));
	if(GET_PTR_VAL(FREE_HEAD(k)) != 0)
		PUT_PTR_VAL(GET_LISTPRE_PTR(GET_TRUE_PTR(GET_PTR_VAL(FREE_HEAD(k)))), GET_PTR_UINT(now));
	PUT_PTR_VAL(GET_LISTPRE_PTR(now), GET_PTR_UINT(FREE_HEAD(k)));
	PUT_PTR_VAL(FREE_HEAD(k), GET_PTR_UINT(now));
}
 
inline void delete_block(uchar *p){
	/*
		p->nex->pre = head;
		head->nex = p->nex;

		p->pre->nex = p->nex
		p->nex->pre = p->pre
	*/
	if((size_t)GET_LISTPRE(p) < (size_t)HEAP_ST){
		size_t k = 0, sz = GET_BLOCK_SIZE(p)>>LOG_ALIGNMENT;
		for(k=0;k<LISTNUM-1&&sz>=UPPER_BOUND(k);k++);
		if(GET_PTR_VAL(GET_LISTNEXT_PTR(p)) != 0){
			PUT_PTR_VAL(GET_LISTPRE_PTR(GET_LISTNEXT(p)),  GET_PTR_UINT(FREE_HEAD(k)));
			PUT_PTR_VAL(FREE_HEAD(k), GET_PTR_VAL(GET_LISTNEXT_PTR(p)));
		}else{
			PUT_PTR_VAL(FREE_HEAD(k),0);
		}
	}else{
		PUT_PTR_VAL(GET_LISTNEXT_PTR(GET_LISTPRE(p)),  GET_PTR_VAL(GET_LISTNEXT_PTR(p)));
		if(GET_PTR_VAL(GET_LISTNEXT_PTR(p)) != 0)
			PUT_PTR_VAL(GET_LISTPRE_PTR(GET_LISTNEXT(p)),  GET_PTR_VAL(GET_LISTPRE_PTR(p)));
	}
}

inline void use_block(uchar *p, uint size){
	uint bsize = GET_BLOCK_SIZE(p);
	if(bsize < size + BARSIZE){
		ADD_PTR_VAL(p, 1);
		ADD_PTR_VAL(GET_FOOTER_PTR(p), 1);
		delete_block(p);
	}else{
		delete_block(p);
		PUT_PTR_VAL(p, size|1);
		PUT_PTR_VAL(GET_FOOTER_PTR(p), size|1);
		uint* nex = GET_NEXT_BLOCK_PTR(p);
		PUT_PTR_VAL(nex, (bsize - size));
		// printf("%p %p %p %ld %ld\n",p,nex,mem_heap_hi(),size,bsize);
		PUT_PTR_VAL(GET_FOOTER_PTR(nex), (bsize - size));
		insert_block(nex);
	}
}

inline void merge_free_blocks(uchar *p, uchar *q){
	uint val = GET_BLOCK_SIZE(p) + GET_BLOCK_SIZE(q);
	delete_block(q);
	delete_block(p);
	PUT_PTR_VAL(p, val);
	PUT_PTR_VAL(GET_FOOTER_PTR(p), val);
	insert_block((uint*)p);
}

void *malloc(size_t size){
	// printf("malloc size=%ld\n",size);
	assert(size > 0);
	uint newsize = ALIGN(size + (WSIZE<<1));
	// printf("malloc size = %ld\n", newsize);
	for(size_t k=0; k<LISTNUM; k++){
		if(k<LISTNUM-1 && (newsize>>LOG_ALIGNMENT) >= UPPER_BOUND(k))continue;
		uint* now = FREE_HEAD(k);
		uint* to;
		if(GET_PTR_VAL(FREE_HEAD(k))!=0){
			// uint* pl = NULL;
			// size_t mx = -1;
			to = GET_TRUE_PTR(GET_PTR_VAL(FREE_HEAD(k)));
			if(GET_BLOCK_SIZE(to) >= newsize){
				// if(mx > (size_t)(to)) mx = (size_t)(to), pl = to;
				use_block((uchar*)to, newsize);
				return GET_RET_PTR(to);
			}
			for(now = to; GET_PTR_VAL(GET_LISTNEXT_PTR(now))!=0; now = to){
				to = GET_LISTNEXT(now);
				if(GET_BLOCK_SIZE(to) >= newsize){
					// if(mx > (size_t)(to)) mx = (size_t)(to), pl = to;
					use_block((uchar*)to, newsize);
					return GET_RET_PTR(to);
				}
			}
			// if(pl!=NULL){
			// 	use_block((uchar*)pl, newsize);
			// 	return GET_RET_PTR(pl);
			// }
		}
	}
	uchar *p = mem_sbrk(newsize);
	if((long)p < 0) return NULL;
	PUT_PTR_VAL(p, newsize);
	PUT_PTR_VAL(GET_FOOTER_PTR(p), newsize);
	insert_block((uint*)p);
	// printf("%d\n",(void*)p!=HEAP_ST);
	if((void*)p!=HEAP_ST && !GET_BLOCK_STATE(GET_PRE_BLOCK_PTR(p))){
		uint *l = GET_PRE_BLOCK_PTR(p);
		merge_free_blocks((uchar*)l, p);
		use_block((uchar*)l, newsize);
		return GET_RET_PTR(l);
	}else{
		use_block(p, newsize);
		return GET_RET_PTR(p);
	}
}

/*
 * free - We don't know how to free a block.  So we ignore this call.
 *      Computers have big memories; surely it won't be a problem.
 */
void free(void *ptr){
	/*Get gcc to be quiet */
	// printf("free ptr=%p\n",ptr);
	if(ptr == NULL) return;
	ptr = GET_HEAD_PTR(ptr);

	// mm_checkheap(0);
	DEL_PTR_VAL(ptr, 1);
	PUT_PTR_VAL(GET_FOOTER_PTR(ptr), GET_PTR_VAL(ptr));
	insert_block((uint*)ptr);
	if((void*)ptr != HEAP_ST && !GET_PRE_BLOCK_STATE(ptr)){
		uint* now = GET_PRE_BLOCK_PTR(ptr);
		merge_free_blocks((uchar *)now,ptr);
		ptr = now;
	}
	if(!END_BLOCK(ptr)){
		uint* nex = GET_NEXT_BLOCK_PTR(ptr);
		if(!GET_BLOCK_STATE(nex))
			merge_free_blocks(ptr,(uchar *)nex);
	}
	// mm_checkheap(0);
}

/*
 * realloc - Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.  I'm too lazy
 *      to do better.
 */
void *realloc(void *oldptr, size_t size){
	// printf("realloc oldptr=%p size=%ld\n",oldptr,size);
	// mm_checkheap(0);
	uint oldsize;
	void *newptr;

	/* If size == 0 then this is just free, and we return NULL. */
	if(size == 0) {
		oldptr = GET_HEAD_PTR(oldptr);
		free(GET_RET_PTR(oldptr));
		return 0; 
	}

  /* If oldptr is NULL, then this is just malloc. */
	if(oldptr == NULL) {
		return malloc(size);
	}

	oldptr = GET_HEAD_PTR(oldptr);
	oldsize = GET_BLOCK_SIZE(oldptr);
	size = ALIGN(size + (WSIZE<<1));
	if(oldsize >= size){
		if(oldsize < size + BARSIZE) return GET_RET_PTR(oldptr);
		PUT_PTR_VAL(oldptr, size|1);
		PUT_PTR_VAL(GET_FOOTER_PTR(oldptr), size|1);
		uint* nex = GET_NEXT_BLOCK_PTR(oldptr);
		PUT_PTR_VAL(nex, (oldsize - size));
		PUT_PTR_VAL(GET_FOOTER_PTR(nex), (oldsize - size));
		insert_block(nex);
		if(!END_BLOCK(nex) && !GET_NEXT_BLOCK_STATE(nex))
			merge_free_blocks((uchar*)nex,(uchar*)(GET_NEXT_BLOCK_PTR(nex)));
		return GET_RET_PTR(oldptr);
	}else{
		size_t sum_size = GET_BLOCK_SIZE(GET_NEXT_BLOCK_PTR(oldptr)) + oldsize;
		if(!END_BLOCK(oldptr) && !GET_NEXT_BLOCK_STATE(oldptr) && sum_size >= size){
			uint* nex = GET_NEXT_BLOCK_PTR(oldptr);
			delete_block((uchar*)nex);
			if(sum_size < size + BARSIZE){
				PUT_PTR_VAL(oldptr, sum_size|1);
				PUT_PTR_VAL(GET_FOOTER_PTR(oldptr), sum_size|1);
				return GET_RET_PTR(oldptr);
			}
			PUT_PTR_VAL(oldptr, size|1);
			PUT_PTR_VAL(GET_FOOTER_PTR(oldptr), size|1);

			nex = GET_NEXT_BLOCK_PTR(oldptr);
			PUT_PTR_VAL(nex, (sum_size - size));
			PUT_PTR_VAL(GET_FOOTER_PTR(nex), (sum_size - size));
			insert_block(nex);
			return GET_RET_PTR(oldptr);
		}else {
			newptr = GET_HEAD_PTR(malloc(size));
			if(!newptr) return 0;
			size = GET_BLOCK_SIZE(newptr) - (WSIZE<<1);
			oldsize = GET_BLOCK_SIZE(oldptr) - (WSIZE<<1);
			if(size < oldsize) oldsize = size;
			memcpy(GET_RET_PTR(newptr) , GET_RET_PTR(oldptr) , oldsize);
			free(GET_RET_PTR(oldptr));
  			return GET_RET_PTR(newptr);
		}
	}
}

/*
 * calloc - Allocate the block and set it to zero.
 */
void *calloc (size_t nmemb, size_t size){
	// printf("calloc nmemb = %ld size = %ld\n", nmemb,size);
	size_t bytes = nmemb * size;
	void *newptr;

	newptr = malloc(bytes);
	memset(newptr, 0, bytes);

  	return newptr;
}

/*
 * mm_checkheap - There are no bugs in my code, so I don't need to check,
 *      so nah!
 */
void mm_checkheap(int verbose){
	/*Get gcc to be quiet. */
	verbose = verbose;
	if(verbose==0){
		puts("---------");
		printf("Rt=%p\n",FREE_HEAD(0));
		printf("FREE_HEAD->%p\n",GET_TRUE_PTR(GET_PTR_VAL(FREE_HEAD(0))));
		for(uchar* p = HEAP_ST;;p = (uchar*)GET_NEXT_BLOCK_PTR(p)){
			printf("--> %p state=%d size=%d tali_size=%d pre=%d next=%d pre_ptr=%p next_ptr=%p\n"
			, p, GET_BLOCK_STATE(p), GET_BLOCK_SIZE(p), GET_BLOCK_SIZE(GET_FOOTER_PTR(p)),
			GET_PTR_VAL(GET_LISTPRE_PTR(p)),GET_PTR_VAL(GET_LISTNEXT_PTR(p)),
			GET_LISTPRE(p),GET_LISTNEXT(p)
			);
			assert(GET_BLOCK_SIZE(p) > 0);
			if(END_BLOCK(p)) break;
		}
		puts("---------");
	}else{
		puts("---------");
		printf("Rt=%p\n",FREE_HEAD(0));
		for(uchar* p = HEAP_ST;;p = (uchar*)GET_NEXT_BLOCK_PTR(p)){
			if(verbose == (long)p)
				printf("--> %p state=%d size=%d tali_size=%d pre=%d next=%d\n"
				, p, GET_BLOCK_STATE(p), GET_BLOCK_SIZE(p), GET_BLOCK_SIZE(GET_FOOTER_PTR(p)),
				GET_PTR_VAL(GET_LISTPRE_PTR(p)),GET_PTR_VAL(GET_LISTNEXT_PTR(p)));
			assert(GET_BLOCK_SIZE(p) > 0);
			if(END_BLOCK(p)) break;
		}
		puts("---------");

	}
}
