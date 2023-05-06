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

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// #define SIZE_PTR(p)  ((size_t*)(((char*)(p)) - SIZE_T_SIZE))

#define GET_PTR_VAL(p) (*((size_t*)(p)))
#define PUT_PTR_VAL(p,val) ((*((size_t*)(p))) = (val))
#define ADD_PTR_VAL(p,val) ((*((size_t*)(p))) |= (val))
#define DEL_PTR_VAL(p,val) ((*((size_t*)(p))) &= (~(val)))

#define GET_BLOCK_SIZE(p)  ((GET_PTR_VAL(p) & ~0x7))
#define GET_BLOCK_STATE(p) ((GET_PTR_VAL(p) & 1))
#define GET_PRE_BLOCK_STATE(p) ((GET_PTR_VAL(p) & 2)>>1)

#define GET_NEXT_BLOCK_PTR(p) ((size_t*)(((char*)(p)) + GET_BLOCK_SIZE(p)))
#define GET_PRE_BLOCK_PTR(p) ((size_t*)( ((char*)(p)) - GET_BLOCK_SIZE((char*)(p) - SIZE_T_SIZE) ))

#define GET_RET_PTR(p) ((void*)((char*)(p) + SIZE_T_SIZE))
#define GET_FOOTER_PTR(p) ((size_t*)((char*)(p) + GET_BLOCK_SIZE(p) - SIZE_T_SIZE))
#define GET_HEAD_PTR(p) ((size_t*)((char*)(p) - SIZE_T_SIZE))

#define END_BLOCK(p) (((long)GET_NEXT_BLOCK_PTR(p)) >= ((long)mem_heap_hi()))


/*
 * mm_init - Called when a new trace starts.
 */

#define HEAP_SIZE 16
int mm_init(void){
	unsigned char *p = mem_sbrk(HEAP_SIZE);
	if((long)p < 0)return -1;
	PUT_PTR_VAL(mem_heap_lo(), HEAP_SIZE);
	PUT_PTR_VAL(GET_FOOTER_PTR(mem_heap_lo()), HEAP_SIZE);
	return 0;
}

/*
 * malloc - Allocate a block by incrementing the brk pointer.
 *      Always allocate a block whose size is a multiple of the alignment.
 */
void use_block(unsigned char *p, size_t size){
	size_t bsize = GET_BLOCK_SIZE(p);
	if(bsize < size + SIZE_T_SIZE + SIZE_T_SIZE){
		ADD_PTR_VAL(p, 1);
		ADD_PTR_VAL(GET_FOOTER_PTR(p), 1);
		if(!END_BLOCK(p)){
			ADD_PTR_VAL(GET_NEXT_BLOCK_PTR(p), 2);
			ADD_PTR_VAL(GET_FOOTER_PTR(GET_NEXT_BLOCK_PTR(p)), 2);
		} 
	}else{
		size_t val = size | 1 | (GET_PRE_BLOCK_STATE(p)<<1);
		PUT_PTR_VAL(p, val);
		PUT_PTR_VAL(GET_FOOTER_PTR(p), val);
		size_t* nex = GET_NEXT_BLOCK_PTR(p);
		PUT_PTR_VAL(nex, (bsize - size) | 2);
		// printf("%p %p %p %ld %ld\n",p,nex,mem_heap_hi(),size,bsize);
		PUT_PTR_VAL(GET_FOOTER_PTR(nex), (bsize - size) | 2);
	}
}

void merge_free_blocks(unsigned char *p, unsigned char *q){
	size_t val = GET_BLOCK_SIZE(p) + GET_BLOCK_SIZE(q);
	val += (GET_PRE_BLOCK_STATE(p)<<1);
	PUT_PTR_VAL(p, val);
	PUT_PTR_VAL(GET_FOOTER_PTR(p), val);
}

void *malloc(size_t size){
	assert(size > 0);
	size_t newsize = ALIGN(size + SIZE_T_SIZE + SIZE_T_SIZE);
	// printf("malloc size = %ld\n", newsize);
	unsigned char *l = mem_heap_lo();
	for(; ; l = (unsigned char*)GET_NEXT_BLOCK_PTR(l)){
		if(!GET_BLOCK_STATE(l)&&GET_BLOCK_SIZE(l) >= newsize){
			use_block(l,newsize);
			return GET_RET_PTR(l);
		}
		if(END_BLOCK(l))break;
	}
	unsigned char *p = mem_sbrk(newsize);
	if((long)p < 0) return NULL;
	PUT_PTR_VAL(p, newsize);
	PUT_PTR_VAL(GET_FOOTER_PTR(p), newsize);
	if(!GET_BLOCK_STATE(l)){
		merge_free_blocks(l,p);
		use_block(l,newsize);
		return GET_RET_PTR(l);
	}else{
		ADD_PTR_VAL(p, 2);
		ADD_PTR_VAL(GET_FOOTER_PTR(p), 2);
		use_block(p,newsize);
		return GET_RET_PTR(p);
	}
}

/*
 * free - We don't know how to free a block.  So we ignore this call.
 *      Computers have big memories; surely it won't be a problem.
 */
void free(void *ptr){
	/*Get gcc to be quiet */
	if(ptr == NULL) return;
	ptr = GET_HEAD_PTR(ptr);
	// printf("free size = %p\n", ptr);
	int flg = 0;
	for(unsigned char *now = mem_heap_lo(); ; now = (unsigned char *)GET_NEXT_BLOCK_PTR(now)){
		if(((void*)now) == ptr && GET_BLOCK_STATE(ptr)){
			flg = 1;
			break;
		}
		if(END_BLOCK(now)) break;
	}
	// if(0x8000cece8==(long)ptr) mm_checkheap(0);
	if(!flg)return;
	DEL_PTR_VAL(ptr, 0x1);
	PUT_PTR_VAL(GET_FOOTER_PTR(ptr), GET_PTR_VAL(ptr));
	if((void*)ptr != mem_heap_lo() && !GET_PRE_BLOCK_STATE(ptr)){
		// mm_checkheap(0);
		size_t* now = GET_PRE_BLOCK_PTR(ptr);
		// printf("%p\n",(void*)now);
		merge_free_blocks((unsigned char *)now,ptr);
		ptr = now;
	}
	if(!END_BLOCK(ptr)){
		size_t* nex = GET_NEXT_BLOCK_PTR(ptr);
		DEL_PTR_VAL(nex, 0x2);
		DEL_PTR_VAL(GET_FOOTER_PTR(nex), 0x2);
		if(!GET_BLOCK_STATE(nex))
			merge_free_blocks(ptr,(unsigned char *)nex);
	}
	// if(0x8000cece8==(long)ptr) exit(0);//mm_checkheap(0);
	// mm_checkheap(0);
}

/*
 * realloc - Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.  I'm too lazy
 *      to do better.
 */
void *realloc(void *oldptr, size_t size){
	// mm_checkheap(0);
	// printf("realloc ptr=%p size=%ld\n", oldptr,size);
	size_t oldsize;
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
  	newptr = GET_HEAD_PTR(malloc(size));

  /* If realloc() fails the original block is left untouched  */
  	if(!newptr) {
   		return 0;
  	}

  /* Copy the old data. */   
	size = GET_BLOCK_SIZE(newptr) - SIZE_T_SIZE - SIZE_T_SIZE;
	oldsize = GET_BLOCK_SIZE(oldptr) - SIZE_T_SIZE - SIZE_T_SIZE;
	if(size < oldsize) oldsize = size;
	// printf("%ld %ld\n", oldsize,size);
	// printf("%p %p\n", (void *)newptr, (void *)oldptr);
	// printf("--->%ld\n",oldsize);
	memcpy(GET_RET_PTR(newptr) , GET_RET_PTR(oldptr) , oldsize);
	// mm_checkheap(0);
  /* Free the old block. */
  	free(GET_RET_PTR(oldptr));
	// mm_checkheap(0);
  	return GET_RET_PTR(newptr);
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
		for(unsigned char* p = mem_heap_lo();;p = (unsigned char*)GET_NEXT_BLOCK_PTR(p)){
			printf("--> %p state=%ld size=%ld pre_state=%ld tali_size=%ld\n"
			, p, GET_BLOCK_STATE(p), GET_BLOCK_SIZE(p), GET_PRE_BLOCK_STATE(p), GET_BLOCK_SIZE(GET_FOOTER_PTR(p)));
			assert(GET_BLOCK_SIZE(p) > 0);
			if(END_BLOCK(p)) break;
		}
		puts("---------");
	}else{
		puts("---------");
		for(unsigned char* p = mem_heap_lo();;p = (unsigned char*)GET_NEXT_BLOCK_PTR(p)){
			if(verbose == (long)p)
			printf("--> %p state=%ld size=%ld pre_state=%ld\n", p, GET_BLOCK_STATE(p), GET_BLOCK_SIZE(p), GET_PRE_BLOCK_STATE(p));
			assert(GET_BLOCK_SIZE(p) > 0);
			if(END_BLOCK(p)) break;
		}
		puts("---------");

	}
}
