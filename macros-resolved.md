# Macros Resolved

| Macro               | Original              | x86 | x64  |
| :------------------ | :-------------------- | :-- | :--- |
| SIZE_T_SIZE         | `sizeof(size_t)`      | 4U  | 8UL  |
| SIZE_T_BITSIZE      | `sizeof(size_t) << 3` | 32U | 64UL |
| SIZE_T_ZERO         | `((size_t)0)`         | 0U  | 0UL  |
| SIZE_T_ONE          | `((size_t)1)`         | 1U  | 1UL  |
| SIZE_T_TWO          | `((size_t)2)`         | 2U  | 2UL  |
| SIZE_T_FOUR         | `((size_t)4)`         | 4U  | 4UL  |
| TWO_SIZE_T_SIZES    | `(SIZE_T_SIZE<<1)`    | 8U  | 16UL |
| FOUR_SIZE_T_SIZES   | `(SIZE_T_SIZE<<2)`    | 16U | 32UL |
| SIX_SIZE_T_SIZES    | `(FOUR_SIZE_T_SIZES+TWO_SIZE_T_SIZES)` | 24U           | 48UL                       |
| MAX_SIZE_T          | `(~(size_t)0)`                         | 4294967295UU  | 18446744073709551615UL     |
| HALF_MAX_SIZE_T     | `(MAX_SIZE_T / 2U)`                    | 4294967295U/2 | 18446744073709551615UL/2UL |
| MALLOC_ALIGNMENT    | `((size_t)(2 * sizeof(void *)))`       | 8U            | 16UL                       |
| CHUNK_ALIGN_MASK    | `MALLOC_ALIGNMENT - SIZE_T_ONE`        | (8-1) = 7U    | (16-1) = 15UL              |
| MFAIL               | `(void*)(MAX_SIZE_T)`                  | `(void*)-1`   | `(void*)-1`                |
| MCHUNK_SIZE         | `sizeof(mchunk)`                       | 16U | 32UL |
| CHUNK_OVERHEAD      | if  FOOTERS: `TWO_SIZE_T_SIZES`        | 8U  | 16UL |
|                     | if !FOOTERS: `SIZE_T_SIZE` (default)   | 4U  | 8UL  |
| MMAP_CHUNK_OVERHEAD | `TWO_SIZE_T_SIZES`                     | 8U  | 16UL |
| MMAP_FOOT_PAD       | `FOUR_SIZE_T_SIZES`                    | 16U | 32UL |
| MIN_CHUNK_SIZE      | `((MCHUNK_SIZE + CHUNK_ALIGN_MASK) & ~CHUNK_ALIGN_MASK)` | ((16+7) & ~7) = 16U | ((32+15) & ~15) = 32U |
| MAX_REQUEST         | `((-MIN_CHUNK_SIZE) << 2)` (interp unsg) | -16 << 2 = -64 | -32 << 2 = -128 |
| MIN_REQUEST         | `(MIN_CHUNK_SIZE - CHUNK_OVERHEAD - SIZE_T_ONE)` | (16-8-1) = 7U | (32-16-1) = 15UL |
| pad_request(req_b)  | `((req_b + CHUNK_OVERHEAD + CHUNK_ALIGN_MASK) & ~CHUNK_ALIGN_MASK)` | `(req_b + 15) & ~7` | `(req_b + 31) & ~15` |
| SMALLBIN_WIDTH      | `SIZE_T_ONE << SMALLBIN_SHIFT`  | 1 << 3 = 8U   | 1 << 3 = 8UL   |
| MIN_LARGE_SIZE      | `(SIZE_T_ONE << TREEBIN_SHIFT)` | 1 << 8 = 256U | 1 << 8 = 256UL |
| MAX_SMALL_SIZE      | `(MIN_LARGE_SIZE - SIZE_T_ONE)` | 255U | 255U |
| MAX_SMALL_REQUEST   | `(MAX_SMALL_SIZE - CHUNK_ALIGN_MASK - CHUNK_OVERHEAD)` | 255-7-8 = 240U | 255-15-8 = 232UL |
