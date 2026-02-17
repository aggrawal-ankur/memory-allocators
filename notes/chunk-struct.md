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

---

We can notice that `head` is a mandatory field while prev_foot/fd/bk are conditional.
  - If the current chunk is an in-use chunk, fd/bk are unnecessary for it. If the chunk previous to it is also an in-use chunk, prev_foot is also unnecessary for it. That means, 12 bytes of memory on x86 and 24 bytes of memory on x64 is getting wasted.
  - If the current chunk is a free chunk and the previous chunk is an in-use chunk, 4/8 bytes of prev_foot are waste on x86/x64.

Memory might not be expensive today, but it used to when the first iteration of dlmalloc was getting written. What's the solution to eliminate this wastage and reduce memory footprint to minimum? The solution looks simple, but my journey to finding it was filled with headaches.

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
