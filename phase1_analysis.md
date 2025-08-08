# Phase 1 Analysis: Performance Baseline and Node Structure

## Task 1: Performance Baseline Metrics

### Current C Implementation Performance (ARM64 macOS)

#### nfib(37) Benchmark Results
- **Average Performance**: 10.88M nfib/s
- **Run 1**: 10.939M nfib/s
- **Run 2**: 10.824M nfib/s  
- **Run 3**: 10.888M nfib/s
- **Execution Time**: ~7.0 seconds for nfib(37)
- **Result**: 78176337 (validates correctness)

#### Sieve of Eratosthenes Benchmark
- **Execution Time**: 0.02s (very fast)
- **Output**: First 100 primes correctly computed
- **Note**: This benchmark is I/O bound, not computation bound

### Target Performance Goals
Based on the plan targets:
- **ARM64 Target**: 40-80M nfib/s (4-8x improvement)
- **x86-64 Target**: 25-55M nfib/s (2.5-5.5x improvement)

## Task 2: Node Structure and Tag Encoding Analysis

### Node Structure Layout (eval.c:466)
```c
typedef struct PACKED node {  // 16 bytes total on 64-bit
  union {                      // 8 bytes
    struct node *uufun;        // Function pointer for T_AP nodes
    intptr_t     uuifun;       // Integer representation
    tag_t        uutag;        // Tag with encoding bits
  } ufun;
  union {                      // 8 bytes
    struct node    *uuarg;     // Argument for applications
    value_t         uuvalue;   // Integer values
    flt64_t         uuflt64value; // Float values
    int64_t         uuint64value; // 64-bit integers
    const char     *uucstring; // C strings
    void           *uuptr;     // Generic pointers
    HsFunPtr        uufunptr;  // Function pointers
    struct ioarray *uuarray;   // Arrays
    struct forptr  *uuforptr;  // Foreign pointers
    struct mthread *uuthread;  // Thread handles
    struct mvar    *uumvar;    // MVars
  } uarg;
} node;
```

### Tag Encoding Scheme (eval.c:494-502)

#### Bit Layout
```
Bits 0-1: Type indicators
  00 = T_AP (application node)
  01 = Tagged value (regular combinator/primitive)
  10 = T_IND (indirection node)
  11 = Invalid (both bits set)

Bits 2-63: Tag value (when bit 0 = 1)
```

#### Key Constants
```c
#define BIT_TAG   1        // Bit 0: Indicates tagged value
#define BIT_IND   2        // Bit 1: Indicates indirection
#define BIT_NOTAP 3        // Bits 0&1: Not an application
#define TAG_SHIFT 2        // Shift to extract tag value
```

#### Tag Extraction Macros
```c
#define GETTAG(p) ((p)->ufun.uutag & BIT_NOTAP ? 
                   ((p)->ufun.uutag & BIT_IND ? T_IND : 
                    (int)((p)->ufun.uutag >> TAG_SHIFT)) : 
                   T_AP)
```

### Critical Tag Values (eval.c:390-428)

#### Special Node Types
- `T_FREE` = 0: Free cell
- `T_IND` = 1: Indirection
- `T_AP` = 2: Application

#### Most Frequent Combinators (per primops[])
1. `T_B` = 18: Composition
2. `T_O` = 25: Composition variant  
3. `T_K` = 16: Constant
4. `T_CC` = 21: C' (flip variant)
5. `T_C` = 19: Flip
6. `T_A` = 20: Application
7. `T_SS` = 22: S' variant
8. `T_P` = 24: Pair

#### Common Arithmetic Primitives
- `T_ADD` = 30: Addition
- `T_SUB` = 31: Subtraction
- `T_MUL` = 32: Multiplication
- `T_QUOT` = 33: Division
- `T_REM` = 34: Remainder

### Assembly Optimization Implications

#### Fast Path Detection
```asm
; ARM64 version
ldr     x7, [x0]            ; Load tag/fun
tst     x7, #3              ; Test BIT_NOTAP (bits 0&1)
b.eq    handle_ap           ; If 00, it's T_AP
tst     x7, #2              ; Test BIT_IND
b.ne    handle_indirection  ; If x10, it's T_IND
; Otherwise it's a regular tag (x01)
lsr     x7, x7, #2          ; Extract tag value
```

#### Memory Layout Optimization
- Node size: exactly 16 bytes (cache-line friendly)
- Alignment: Natural 8-byte alignment for both fields
- Tag in first word allows prefetching before dispatch

### Critical Performance Patterns

#### Application Node (T_AP) - Most Common Case
- No tag bits set (bits 0-1 = 00)
- Direct pointer in first word
- ~20% of all evaluations

#### Indirection Following
- Bit pattern: x10 
- Requires pointer masking: `ptr & ~BIT_IND`
- Common after updates

#### Combinator Dispatch
- Bit pattern: x01
- Tag value in bits 2-63
- Requires right shift by 2 to get combinator ID

## Summary and Next Steps

### Key Findings
1. **Baseline Performance**: 10.88M nfib/s needs 4-8x improvement
2. **Node Structure**: 16-byte aligned, tag in LSBs of first word
3. **Hot Path**: T_AP (00), then tagged combinators (01), then T_IND (10)
4. **Priority Combinators**: B, O, K, C', C based on frequency

### Recommendations for Assembly Implementation
1. Use conditional moves/selects to avoid branch misprediction
2. Inline the most frequent combinators (B, O, K)
3. Keep dispatch table in cache by minimizing size
4. Use prefetch instructions for node traversal
5. Optimize for the T_AP fast path (no tag bits)

### Phase 2 Prerequisites
- Set up assembly build infrastructure
- Create test harness for correctness validation
- Implement core dispatch loop
- Start with B, O, K combinators per frequency data