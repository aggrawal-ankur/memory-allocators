#include <stdio.h>

struct malloc_chunk {
  size_t               prev_foot;  /* Size of previous chunk (if free).  */
  size_t               head;       /* Size and inuse bits. */
  struct malloc_chunk* fd;         /* double links -- used only if free. */
  struct malloc_chunk* bk;
};

typedef struct malloc_chunk  mchunk;

typedef unsigned int flag_t;

struct malloc_segment {
  char*                  base;     /* base address */
  size_t                 size;     /* allocated size */
  struct malloc_segment* next;     /* ptr to next segment */
  flag_t                sflags;    /* mmap and extern flag */
};

int main(void){
  printf("sizeof(size_t): %d\n", sizeof(size_t));
  printf("sizeof(size_t) << 3: %d\n", sizeof(size_t) << 3);
  printf("(size_t)0, sizeof((size_t)0): %d, %d\n", (size_t)0, sizeof((size_t)0));
  printf("(size_t)1, sizeof((size_t)1): %d, %d\n", (size_t)1, sizeof((size_t)1));
  printf("(size_t)2, sizeof((size_t)2): %d, %d\n", (size_t)2, sizeof((size_t)2));
  printf("(size_t)4, sizeof((size_t)4): %d, %d\n", (size_t)4, sizeof((size_t)4));
  printf("sizeof(size_t) << 1, sizeof(sizeof(size_t) << 1): %d, %d\n", sizeof(size_t) << 1, sizeof(sizeof(size_t) << 1));
  printf("sizeof(size_t) << 2, sizeof(sizeof(size_t) << 2): %d, %d\n", sizeof(size_t) << 2, sizeof(sizeof(size_t) << 2));
  printf("~(size_t)0: %zu\n", ~(size_t)0);
  printf("(size_t)(2 * sizeof(void *)): %d\n", (size_t)(2 * sizeof(void *)));
  printf("sizeof(mchunk): %d\n", sizeof(mchunk));
  printf("sizeof(struct malloc_segment): %d\n", sizeof(struct malloc_segment));
}
