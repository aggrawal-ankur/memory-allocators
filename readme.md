# Memory Allocator

***February 14, 2026***

I am exploring how memory allocators work on Linux, primarily dlmalloc and ptmalloc(2 and 3).

I am starting with dlmalloc, my old friend.

The first thing I have done is to shift the prologue and epilogue of annotations to separate files so that I don't have to scroll a lot to reach the main thing every time I open the file.

I've obtained the original source from [DenizThatMenace/dlmalloc](https://github.com/DenizThatMenace/dlmalloc). I am using v2.8.6 .

The next thing I did is improving the annotations for readability. Also, I've cut the upper part to malloc.h, as mentioned in the source. This helps in further reducing the source to the main logic only.

The next thing I am doing is macro resolution for x64-linux as the architecture.

---

***February 15, 2026***

Continuing my macro-resolution journey, I've encountered the "chunk representation" annotations, which I have shifted into a separate file. I've formatted them a little, but I have some other ideas to improve them as per my understanding of chunks.

I am done with this macro-resolution and code formatting part and the next thing is to dive into chunks and see how the memory looks like.

Earlier I've used a 10x10 grid made up of box-drawing elements to represent 100 blocks (bytes) of memory. This time, I am scaling it to real world usage.

[mem-art](./notes/mem-art) contains an ASCII grid representing a 4KiB page worth of memory. I've used this python script to generate it:
```py
num=0
for i in range(128):
  print("┌────"*32, "┐", sep='')
  print("|    "*32, "|", sep='')
  print("└────"*32, "┘", sep='')

  for j in range(32):
    if (num >= 0 and num <= 10):
      print(f"  {num}  ", end='')
    elif (num >= 11 and num <= 99):
      print(f" {num}  ", end='')
    elif (num >= 100 and num <= 1000):
      print(f" {num} ", end='')
    elif (num >= 1001):
      print(f" {num}", end='')
    num += 1
  
  print()
```

Why I have scaled to 4096 bytes only? Because that's the minimum amount of memory the kernel can release on 4kiB systems.
