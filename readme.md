# Memory Allocator

***February 14, 2026***

I am exploring how memory allocators work on Linux, primarily dlmalloc and ptmalloc(2 and 3).

I am starting with dlmalloc, my old friend.

The first thing I have done is to shift the prologue and epilogue of annotations to separate files so that I don't have to scroll a lot to reach the main thing every time I open the file.

I've obtained the original source from [DenizThatMenace/dlmalloc](https://github.com/DenizThatMenace/dlmalloc). I am using v2.8.6 .

The next thing I did is improving the annotations for readability. Also, I've cut the upper part to malloc.h, as mentioned in the source. This helps in further reducing the source to the main logic only.
