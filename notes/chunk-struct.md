# Everything About Chunk

This is how a chunk looks like:
```c
struct malloc_chunk {
  size_t               prev_foot;
  size_t               head;
  struct malloc_chunk* fd;
  struct malloc_chunk* bk;
};
```

A chunk is just a piece of metadata that is associated with a piece of memory.

Understanding the size of this metadata struct is the real deal. On surface, it looks like this:

| Arch | Size |
| :--- | :--- |
| 32-bit (or x86) | 16 bytes |
| 64-bit (or x64) | 32 bytes |

It is based on the value of `size_t` in an architecture, which is 4-bytes for 32-bit systems and 8-bytes for 64-bits, as these are the maximum addressable lengths in their respective systems.

But that's incomplete. Why? Remember, dlmalloc was written in a time when memory was scarce and every byte mattered. Professor Doug Lea optimized for every single byte, which is what that makes dlmalloc somewhat complicated, and understanding this metadata chunk is paramount to understanding the overall memory layout.

## head

We have two chunk types: **in-use** and **free**. If you notice, the malloc_chunk itself doesn't have anything that can distinguish b/w a free chunk and an in-use chunk. So what creates the difference?

---

dlmalloc aligns chunks to double-word boundaries (8 bytes on 32-bit, 16 bytes on 64-bit) as the C standard mandates malloc to return a pointer which is suitably aligned for any object type.

For this reason, the allocator must choose an alignment that's sufficient for the largest primitive type that could appear on the target architecture.
  - On 64-bit systems, that's typically 16 bytes due to `long double` or `SIMD` instructions.
  - On 32-bit systems, it is 8 bytes for double.

Any number which is a multiple of 8/16 does not uses the lower 3 bits, (i.e 0, 1, 2). These bits are always clear(0). dlmalloc uses bit-manipulation to cleverly use these bits to mask `pinuse` and `cinuse` bits. The third bit is not used by dlmalloc and is left for future extensions.

---

`head` stores the "total size of the metadata chunk and the payload memory" rounded up to a 8/16 bytes memory on (32-bit/64-bit) systems.

The `CINUSE` bit is the 0th bit in the `head` element, which specifies the type of the current chunk.
  - 0 ⇒ current chunk is a free chunk.
  - 1 ⇒ current chunk is an in-use chunk.

The `PINUSE` bit specifies the type of the previous chunk.
  - 0 ⇒ previous chunk is a free chunk.
  - 1 ⇒ previous chunk is an in-use chunk.

---

Operations on `head` are simple.

| Data To Retrieve | Retrieval Mechanism |
| :--------------- | :------------------ |
| cinuse bit | `head & 0x1`  |
| pinuse bit | `head & 0x2`  |
| chunk size | `head & ~0x7` |

---

`head` is only the head of the-dlmalloc-iceberg.

## prev_foot/fd/bk

prev_foot stores the size of the previous chunk if it is a free chunk, else it is garbage.
  - If the pinuse bit is 0, the previous chunk is a free chunk and prev_foot stores the size of it.
  - If the pinuse bit is 1, the previous chunk is an in-use chunk and prev_foot is garbage.

fd stands for "forward" and bk stands for "backward". These are pointers to "free chunks" after and before the current chunk, respectively. In case of in-use chunks, fd/bk are garbage.

## dlmalloc's Memory Layout

We can notice that `head` is a mandatory field while prev_foot/fd/bk are conditional.
  - If the current chunk is an in-use chunk, fd/bk are unnecessary. If the chunk previous to it is also an in-use chunk, prev_foot is also unnecessary. That means, 12 bytes of memory on x86 and 24 bytes of memory on x64 is getting wasted.
  - If the current chunk is a free chunk and the previous chunk is an in-use chunk, 4/8 bytes of prev_foot are wasted on x86/x64.

Memory might not be expensive today, but it was when dlmalloc was being written. How can we eliminate this wastage and reduce memory footprint to minimum? The solution looks simple, but my journey to finding it was filled with headaches.

This is a grid that represents byte-addressable memory:
```
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
|    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15 
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
|    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  16   17   18   19   20   21   22   23   24   25   26   27   28   29   30   31
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
|    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  32   33   34   35   36   37   38   39   40   41   42   43   44   45   46   47 
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
|    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  48   49   50   51   52   53   54   55   56   57   58   59   60   61   62   63  
```
Each block is 1 byte of memory.

Normally we would assume this layout:
```
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| p  | r  | e  | v  | f  | o  | o  | t  | h  | .. | e  | .. | a  | .. | .. | d  |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15 
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| f. | .. | .. | .. | .. | .. | .. | .d | b. | .. | .. | .. | .. | .. | .. | .k |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  16   17   18   19   20   21   22   23   24   25   26   27   28   29   30   31
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| P  | A  | Y  | L  | O  | A  | D  |    | M  | E  | M  | O  | R  | Y  |    |    |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  32   33   34   35   36   37   38   39   40   41   42   43   44   45   46   47 
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| R  | E  | T  | U  | R  | N  | E  | D  |    | T  | O  |    | U  | S  | E  | R  |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  48   49   50   51   52   53   54   55   56   57   58   59   60   61   62   63  
```
But that's not how dlmalloc does it.

In simple words, fd/bk are repurposed to represent the start of the payload memory and prev_foot starts from the boundary within the previous chunk's payload memory when these fields are not important. This sounds illogical for a variety of reasons.
  - How can the malloc_chunk struct start from within the previous chunk's payload memory?
  - How can fd/bk be repurposed to represent the start of the payload memory?
  - Doesn't that violate struct integrity?

We will answer these questions in two parts.
  - Part 1 will give a theoretical answer.
  - Part 2 will prove the theoretical answer visually through an ASCII grid.

### Part 1

***How can the malloc_chunk struct start from within the previous chunk's payload memory?***
  - We are not allocating on stack where the compiler manages the layout. We are allocating on heap, and as long as the memory is valid and accessible, the struct can virtually start anywhere in the memory.
  - As long as the memory layout is concerned, that one struct comes after the other and can't overlay, that's a clever way dlmalloc reduces memory footprint and this is something the visual layout will clear better.

***How can fd/bk be repurposed to represent the start of the payload memory?***
  - That's another way dlmalloc utilizes memory to the best of its capabilities.
  - To the user that requests malloc memory, the chunk metadata doesn't concern them. All they want is memory, so a pointer to that memory is returned to the user.
  - fd/bk are garbage for in-use chunks. Either we allot dedicated space to them, which will still be garbage, or we repurpose them by returning a pointer to `X+16` instead of `X+32` where the struct ends.
  - Take it this way: what does it mean for a pointer to be garbage? **It shouldn't be dereferenced**. In either of the cases, we are not populating the memory by dereferencing fd/bk. We are populating the memory by dereferencing the payload pointer, it just happens to be same for the initial 16 bytes, that's it. In either of the cases, dereferencing fd/bk is not defined.
  - We are not breaking any rule, we are just cleverly using them. If a chunk is in-use, dlmalloc simply doesn't use fd/bk. That's it.

It took me enough time to figure this out because it is complicated. We don't assume memory is overlaid or repurposed. But in the end, it all comes down to following the rules and dlmalloc follows them. It doesn't violate them by this clever usage. That's the highlight.

### Part 2

To visualize the memory layout, we will take an example that requests and frees the heap. This way, we can understand how chunks come into existence.

We will perform three malloc(s) and free(s):
```
p = malloc(10);
q = malloc(25);
r = malloc(15);

free(q);
free(r);
```

---

Let's start with the first one: malloc(10).
  - As mentioned earlier, the kernel releases memory in page-granularity. The minimum it can release is one page worth of memory. Mainstream Linux has 4 KiB pages. To fathom that much memory, open [mem-art](./mem-art).
  - When dlmalloc receives memory from the kernel, a segment is formed, from which it serves the process's heap requirements. When dlmalloc demands more memory from the kernel, the kernel releases it and this segment's size increases. This is the case for MORECORE. If it is MMAP, every kernel release creates a new segment.

CHUNK_OVERHEAD refers to the size of the metadata chunk. As we know it now, it varies.
  - For an in-use, fd/bk are garbage and repurposed, prev_foot depends on the pinuse bit and head is mandatory.
  - On x64, that makes the minimum CHUNK_OVERHEAD 8 bytes and the maximum 16 bytes for an in-use chunk.

The first chunk is special as it has the pinuse bit set (1). As per the annotations, the first chunk always has the pinuse bit set so that access to non-existent or non-owned memory can be prevented. That means, prev_foot is garbage. But there is no chunk before the first chunk, that's why the chunk overhead in this case is 16 bytes, because we have to allocate memory to prev_foot even if garbage.

So, CHUNK_OVERHEAD=16 bytes. Add the requested amount of bytes to it, we get (16 + 10) 26 bytes.
  - 26 bytes are not aligned to a 16-divisible boundary, so we need some DWORD_PADDING.
  - We need (32-26) 6 bytes of padding to round 26 up to the next 16-divisible boundary, i.e 32.

That's how malloc(10) becomes malloc(32). Let's plot this in the ASCII memory grid.
```
  g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| p1 | r1 | e1 | v1 | f1 | o1 | o1 | t1 | h1 | .. | e1 | .. | a1 | .. | .. | d1 |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15 

  g    a    r    b    b    a    g    e    g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| f1 | .. | .. | .. | .. | .. | .. | 1d | b1 | .. | .. | .. | .. | .. | .. | 1k |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  p.                                                                         .p
  P    A    Y    L    O    A    D    ..   ..   ..   M    E    M    O    R    Y
  16   17   18   19   20   21   22   23   24   25   26   27   28   29   30   31
```

That's the first piece of utilization in dlmalloc. The payload memory starts where the fd pointer starts. We've achieved all the outcomes, alongside less memory footprint:
  - fd/bk still exist. The layout is not violated,
  - fd/bk are garbage as per the in-use chunk idea, and
  - dereferencing is not defined on fd/bk.

You can notice that we have requested 10 bytes but getting 16 bytes of writable memory. That's the reason why we sometimes don't hit undefined behavior even if accidentally write past the allocated memory.

---

It's time for malloc(25). Since it is the second chunk, and the chunk previous to it is an in-use chunk, so the pinuse bit for it is set(1), which means prev_foot is garbage.

Let's calculate the total allocation size:
  - CHUNK_OVERHEAD=8 bytes.
  - CHUNK_OVERHEAD + REQUESTED_BYTES = 8 + 25 = 33 bytes
  - DWORD_PADDING = (48-33) 15 bytes.
  - Total size =: 48 bytes

This makes malloc(25) -> malloc(48).

CHUNK_OVERHEAD being 8 bytes is the second highlight of dlmalloc's memory utilization. Let's plot this in the ASCII memory grid.
```
  g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| p1 | r1 | e1 | v1 | f1 | o1 | o1 | t1 | h1 | .. | e1 | .. | a1 | .. | .. | d1 |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15 

  g    a    r    b    b    a    g    e    g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| f1 | .. | .. | .. | .. | .. | .. | 1d | b1 | .. | .. | .. | .. | .. | .. | 1k |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  p.                                                                         .p
  P    A    Y    L    O    A    D    ..   ..   ..   M    E    M    O    R    Y
                                          p2   r2   e2   v2   f2   o2   o2   t2
                                          g    a    r    b    b    a    g    e
  16   17   18   19   20   21   22   23   24   25   26   27   28   29   30   31

                                          g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| h2 | .. | e2 | .. | a2 | .. | .. | d2 | f2 | .. | .. | .. | .. | .. | .. | 2d |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
                                          q.
                                          P    ..   A    ..   Y    ..   L    ..
  32   33   34   35   36   37   38   39   40   41   42   43   44   45   46   47 

  g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| b2 | .. | .. | .. | .. | .. | .. | 2k | -- | -- | -- | -- | -- | -- | -- | -- |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  O    ..   A    ..   D    ..   ..   M    ..   ..   E    ..   ..   M    ..   ..
  48   49   50   51   52   53   54   55   56   57   58   59   60   61   62   63  

┌────┌────┌────┌────┌────┌────┌────┌────┐
| -- | -- | -- | -- | -- | -- | -- | -- |
└────└────└────└────└────└────└────└────┘
                                     .q
  ..   O    ..   ..   R    ..   ..   Y
  64   65   66   67   68   69   70   71

```
`--` represents payload memory beyond fd/bk. I am using this notation to contain future confusion.

Notice that the prev_foot of this chunk is starting 8 bytes inside the boundary of the payload memory of the first chunk. It is not violating as prev_foot is still garbage and dereferencing is not defined. Again, we have achieved the outcome at lesser memory footprint.

---

It's time for the third malloc, malloc(15).

Now we know that CHUNK_OVERHEAD is 8 bytes, 8+15 is 23 bytes and 9 bytes of padding makes malloc(15) -> malloc(32). Let's plot it.

```
  g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| p1 | r1 | e1 | v1 | f1 | o1 | o1 | t1 | h1 | .. | e1 | .. | a1 | .. | .. | d1 |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15 

  g    a    r    b    b    a    g    e    g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| f1 | .. | .. | .. | .. | .. | .. | 1d | b1 | .. | .. | .. | .. | .. | .. | 1k |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  p.                                                                         .p
  P    A    Y    L    O    A    D    ..   ..   ..   M    E    M    O    R    Y
                                          p2   r2   e2   v2   f2   o2   o2   t2
                                          g    a    r    b    b    a    g    e
  16   17   18   19   20   21   22   23   24   25   26   27   28   29   30   31

                                          g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| h2 | .. | e2 | .. | a2 | .. | .. | d2 | f2 | .. | .. | .. | .. | .. | .. | 2d |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
                                          q.
                                          P    ..   A    ..   Y    ..   L    ..
  32   33   34   35   36   37   38   39   40   41   42   43   44   45   46   47 

  g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| b2 | .. | .. | .. | .. | .. | .. | 2k | -- | -- | -- | -- | -- | -- | -- | -- |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  O    ..   A    ..   D    ..   ..   M    ..   ..   E    ..   ..   M    ..   ..
  48   49   50   51   52   53   54   55   56   57   58   59   60   61   62   63  

┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| -- | -- | -- | -- | -- | -- | -- | -- | h3 | .. | e3 | .. | a3 | .. | .. | d3 |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
                                     .q
  ..   O    ..   ..   R    ..   ..   Y
  p3   r3   e3   v3   f3   o3   o3   t3
  g    a    r    b    b    a    g    e
  64   65   66   67   68   69   70   71   72   73   74   75   76   77   78   79 

  g    a    r    b    b    a    g    e    g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| f3 | .. | .. | .. | .. | .. | .. | 3d | b3 | .. | .. | .. | .. | .. | .. | 3k |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  r.                                                                         .r
  P    A    Y    L    O    A    D    ..   ..   ..   M    E    M    O    R    Y
  80   81   82   83   84   85   86   87   88   89   90   91   92   93   94   95  
```

Done. It's time to free this memory.

---

free(q)

```
  g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| p1 | r1 | e1 | v1 | f1 | o1 | o1 | t1 | h1 | .. | e1 | .. | a1 | .. | .. | d1 |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15 

  g    a    r    b    b    a    g    e    g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| f1 | .. | .. | .. | .. | .. | .. | 1d | b1 | .. | .. | .. | .. | .. | .. | 1k |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  p.                                                                         .p
  P    A    Y    L    O    A    D    ..   ..   ..   M    E    M    O    R    Y
                                          p2   r2   e2   v2   f2   o2   o2   t2
                                          g    a    r    b    b    a    g    e
  16   17   18   19   20   21   22   23   24   25   26   27   28   29   30   31

                                          D    E    R    E    F    E    R    E
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| h2 | .. | e2 | .. | a2 | .. | .. | d2 | f2 | .. | .. | .. | .. | .. | .. | 2d |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  32   33   34   35   36   37   38   39   40   41   42   43   44   45   46   47 

  N    C    I    N    G    DE   FI   NED
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| b2 | .. | .. | .. | .. | .. | .. | 2k | U  | N  | A  | L  | L  | O  | C  | A  |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  48   49   50   51   52   53   54   55   56   57   58   59   60   61   62   63  

┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| T  | E  | D  | M  | E  | M  | R  | Y  | h3 | .. | e3 | .. | a3 | .. | .. | d3 |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  p3   r3   e3   v3   f3   o3   o3   t3
  D    E    F    I    N    E    D    D
  64   65   66   67   68   69   70   71   72   73   74   75   76   77   78   79 

  g    a    r    b    b    a    g    e    g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| f3 | .. | .. | .. | .. | .. | .. | 3d | b3 | .. | .. | .. | .. | .. | .. | 3k |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  r.                                                                         .r
  P    A    Y    L    O    A    D    ..   ..   ..   M    E    M    O    R    Y
  80   81   82   83   84   85   86   87   88   89   90   91   92   93   94   95  
```

`//` means unallocated memory, the memory which used to be allocated but is freed now.

The chunk previous to this chunk is still an in-use chunk, so prev_foot is still garbage and if you notice, it is still garbage. However, fd/bk are important to free chunks. For the time being, let's not ask what will fd/bk contain, just accept that they aren't garbage, they are properly managed and dereferencing is now defined.

Since this chunk is previous to the third chunk(r), the head of that chunk is updated with pinuse bit being cleared(0), which means, prev_foot is now managed and contains the size of the chunk that is freed now (q). But, here is the twist. prev_foot for the third chunk is defined in the territory of the previous chunk, if prev_foot of the third chunk is defined, doesn't that reduces the size of the second chunk? It will not.
  - The answer is in interpretation.
  - When prev_foot of (r) is updated with the size of (q), it does occupy the space, but when (q) is reallocated, the pinuse bit in (r) is flipped again and prev_foot of (r) becomes garbage again. That's it.

That's what it means by "the boundary tag method". In free chunks, the size is maintained in their head and the foot (the prev_foot of the next chunk), and when the chunk next to a free chunk is freed, backward coalescing can happen and the two chunks can be joined easily with the minimum overhead possible, creating one single chunk of combined size. We'll do that next.

---

free(r)

```
  g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| p1 | r1 | e1 | v1 | f1 | o1 | o1 | t1 | h1 | .. | e1 | .. | a1 | .. | .. | d1 |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15 

  g    a    r    b    b    a    g    e    g    a    r    b    b    a    g    e
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| f1 | .. | .. | .. | .. | .. | .. | 1d | b1 | .. | .. | .. | .. | .. | .. | 1k |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  p.                                                                         .p
  P    A    Y    L    O    A    D    ..   ..   ..   M    E    M    O    R    Y
                                          p2   r2   e2   v2   f2   o2   o2   t2
                                          g    a    r    b    b    a    g    e
  16   17   18   19   20   21   22   23   24   25   26   27   28   29   30   31

                                          D    E    R    E    F    E    R    E
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| h2 | .. | e2 | .. | a2 | .. | .. | d2 | f2 | .. | .. | .. | .. | .. | .. | 2d |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  32   33   34   35   36   37   38   39   40   41   42   43   44   45   46   47 

  N    C    I    N    G    DE   FI   NED
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| b2 | .. | .. | .. | .. | .. | .. | 2k | U  | N  | A  | L  | L  | O  | C  | A  |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  48   49   50   51   52   53   54   55   56   57   58   59   60   61   62   63  

┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| T  | E  | D  | M  | E  | M  | R  | Y  | h3 | .. | e3 | .. | a3 | .. | .. | d3 |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  p3   r3   e3   v3   f3   o3   o3   t3
  D    E    F    I    N    E    D    D
  64   65   66   67   68   69   70   71   72   73   74   75   76   77   78   79 

  D    E    R    E    F    E    R    E    N    C    I    N    G    DE   FI   NED
┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┌────┐
| f3 | .. | .. | .. | .. | .. | .. | 3d | b3 | .. | .. | .. | .. | .. | .. | 3k |
└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────└────┘
  80   81   82   83   84   85   86   87   88   89   90   91   92   93   94   95  
```

You may ask what do we even get by freeing r, it is 32 bytes in size and all that is occupied by metadata. The question may sound trivial but nothing in dlmalloc is trivial. Considering a question trivial and not asking is the biggest wrong thing you can do here.
  - Once these 32-bytes chunk are reallocated, fd/bk becomes garbage again. If the chunk previous to them is not free anymore, the prev_foot is garbage again. 16 bytes of memory is always available.
  - It is about interpretation.

Now we have two free chunks lying together, we can perform backward coalescing here and make a large chunk of size (32+48) 80 bytes.