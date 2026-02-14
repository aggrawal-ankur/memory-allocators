#include <stdio.h>

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
}
