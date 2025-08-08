# Implementation Plan: Optimized MicroHs Evaluator Loop

## Executive Summary

This document outlines the plan for optimizing the critical evaluation loop in MicroHs runtime (`evali()` function) using assembly language, while maintaining the existing C infrastructure for memory management, I/O, and other non-critical paths. The approach prioritizes platform independence by providing architecture-specific optimized versions alongside the portable C implementation.

### Goals
- **Performance Target**: 5-10x speedup on nfib benchmark (from ~8M to 40-80M nfib/s)
- **Compatibility**: Pass all existing tests without modification
- **Architecture**: Primary target ARM64, secondary target x86-64
- **Platform Independence**: Maintain portable C version, add architecture-specific optimized variants
- **Scope**: Focus on `evali()` tight loop optimization, keep GC and complex operations in C
- **Validation**: Use existing benchmark suite from `benchmark.md`

## Phase 1: Analysis and Architecture Design (Week 1)

### 1.1 Scope Definition
Optimization focus on `evali()` tight loop only:
- **Target Function**: Core evaluation loop in `evali()` (~2400 lines)
- **Retained in C**: Memory management, GC, I/O, complex primitives, initialization
- **Assembly Optimization**: Hot path dispatch, common combinators, stack operations
- **Integration Model**: Assembly functions called from C framework

### 1.2 Performance Critical Paths
Based on profiling, the hotspots in `evali()` are:
1. **Node dispatch** (switch on tag) - 40% of execution time
2. **Stack operations** (PUSH/POP macros) - 15%
3. **Application nodes** (T_AP) evaluation - 20%
4. **Common combinators** (I, K, S, B, C) - 15%
5. **Integer arithmetic** - 10%

**Implementation Priority**: The `primops[]` array in eval.c:1759 is sorted by frequency of occurrence in typical programs. This ordering will guide our implementation priority:
- Combinators: B, O, K, C', C, A, S', P, R, I, S (in frequency order)
- Arithmetic: +, -, *, quot, rem (in frequency order)
- This ensures we optimize the most impactful operations first

### 1.3 Assembly Architecture Design

#### Register Allocation Strategy (ARM64 - Primary Target)
```
x0  - Current node pointer (n)
x1  - Stack pointer (custom stack)
x2  - Temporary/argument x
x3  - Temporary/argument y  
x4  - Temporary/argument z
x5  - Global slice counter
x6  - Heap base (cells)
x7  - Scratch/dispatch target
x19-x28 - Preserved: cached combinators/roots
x29 - Frame pointer
x30 - Link register
```

#### Register Allocation Strategy (x86-64 - Secondary Target)
```
%rdi - Current node pointer (n)
%rsi - Stack pointer (custom stack, not %rsp)
%rdx - Temporary/argument x
%rcx - Temporary/argument y
%r8  - Temporary/argument z
%r9  - Global slice counter
%r10 - Heap base (cells)
%r11 - Scratch/indirect jump target
%rbx - Preserved: GC roots
%r12-r15 - Preserved: cached combinators
```

### 1.4 Integration Strategy

#### Build System Approach
```makefile
# Architecture detection and selection
bin/mhseval: src/runtime/eval.c
    $(CC) -o $@ $< $(CFLAGS)  # Standard C version

bin/mhseval-arm64: src/runtime/eval.c src/runtime/eval_arm64.s
    $(CC) -DUSE_ASM_EVAL -o $@ $^ $(CFLAGS)

bin/mhseval-x86_64: src/runtime/eval.c src/runtime/eval_x86_64.s  
    $(CC) -DUSE_ASM_EVAL -o $@ $^ $(CFLAGS)
```

#### C/Assembly Interface
```c
// In eval.c
#ifdef USE_ASM_EVAL
extern value_t evali_asm(value_t n, value_t* stack);
#define evali_core evali_asm
#else
#define evali_core evali_c
#endif
```

## Phase 2: ARM64 Implementation (Weeks 2-4)

### 2.1 Main Evaluation Loop (ARM64)

Optimized dispatch using computed goto:

```asm
.global evali_asm
evali_asm:
    // Function prologue
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    stp     x19, x20, [sp, #-16]!
    
eval_loop:
    // Check slice counter
    subs    x5, x5, #1
    b.eq    yield_to_c
    
    // Load and decode tag
    ldr     x7, [x0]            // Load tag/fun
    tst     x7, #NODE_TAG_BIT
    b.eq    handle_ap           // Branch if application
    tst     x7, #NODE_IND_BIT  
    b.ne    follow_indirection
    
    // Dispatch table jump
    lsr     x7, x7, #2          // Extract tag
    adr     x8, dispatch_table
    ldr     x8, [x8, x7, lsl #3]
    br      x8
```

### 2.2 Fast Path Optimizations (ARM64)

#### Application Node Handling
```asm
handle_ap:
    str     x0, [x1], #8        // Push node on stack
    ldr     x0, [x0]            // n = FUN(n)
    b       eval_loop
```

#### Combinator Implementation Priority
Based on `primops[]` frequency ordering in eval.c:1759, implement in this order:

**Phase 1 - Most Frequent (Must Have)**:
1. **B** - Composition combinator
2. **O** - Composition variant
3. **K** - Constant function
4. **C'** - Flip variant
5. **C** - Flip combinator
6. **A** - Application
7. **S'** - Substitution variant
8. **P** - Pair constructor

**Phase 2 - Common (Should Have)**:
9. **R** - Rotation
10. **I** - Identity
11. **S** - Substitution
12. **U** - Uncurry

**Phase 3 - Less Frequent (Nice to Have)**:
13. **Y** - Fixed point
14. **B'** - Composition variant
15. **Z** - Lazy fixed point

Example implementations for top combinators:
```asm
comb_B:  // B f g x = f (g x)
    cmp     x1, stack_base_plus_16
    b.lo    eval_return
    // Implementation follows frequency-first principle
    
comb_K:  // K x y = x
    cmp     x1, stack_base_plus_8
    b.lo    eval_return
    ldr     x0, [x1, #-8]       // TOP(0)
    ldr     x0, [x0, #8]        // ARG(TOP(0))
    sub     x1, x1, #16         // POP(2)
    str     x0, [x1, #-8]       // Update node
    b       eval_loop
```

### 2.3 Arithmetic Fast Paths (ARM64)

Priority based on `primops[]` frequency (eval.c:1782+):
1. **+** (ADD) - Most frequent arithmetic
2. **-** (SUB) 
3. ***** (MUL)
4. **quot**, **rem** - Division operations
5. Boolean: **and**, **or**, **xor**

Implementation focus on most frequent:

```asm
prim_add:
    // Check stack depth
    mov     x8, stack_base_plus_16
    cmp     x1, x8
    b.lo    call_c_fallback
    
    // Inline integer evaluation for common case
    ldr     x2, [x1, #-8]       // TOP(0)
    ldr     x2, [x2, #8]        // ARG(TOP(0))
    ldr     x8, [x2]            // Check if already int
    tbnz    x8, #0, eval_arg1   // Not a value, need eval
    ldr     x2, [x2, #8]        // Get int value
    
    ldr     x3, [x1, #-16]      // TOP(1) 
    ldr     x3, [x3, #8]        // ARG(TOP(1))
    ldr     x8, [x3]            // Check if already int
    tbnz    x8, #0, eval_arg2   // Not a value, need eval
    ldr     x3, [x3, #8]        // Get int value
    
    // Fast path addition
    adds    x2, x2, x3
    b.vs    call_c_overflow     // Overflow to C handler
    
    // Update result
    sub     x1, x1, #16         // POP(2)
    ldr     x0, [x1, #-8]       // n = TOP(-1)
    mov     x8, #(T_INT << 2) | NODE_TAG_BIT
    stp     x8, x2, [x0]        // Store tag and value
    b       eval_return
```

### 2.4 C Fallback Interface

Clean interface to C for complex operations:

```asm
call_c_fallback:
    // Save ARM64 state
    stp     x0, x1, [sp, #-16]!
    stp     x2, x3, [sp, #-16]!
    stp     x4, x5, [sp, #-16]!
    
    // Call C implementation
    bl      evali_c_complex
    
    // Restore state
    ldp     x4, x5, [sp], #16
    ldp     x2, x3, [sp], #16
    ldp     x0, x1, [sp], #16
    b       eval_loop

yield_to_c:
    // Clean return to C caller
    ldp     x19, x20, [sp], #16
    ldp     x29, x30, [sp], #16
    ret
```

## Phase 3: x86-64 Implementation (Weeks 5-6)

### 3.1 Main Evaluation Loop (x86-64)

```asm
.global evali_asm
evali_asm:
    push    %rbp
    mov     %rsp, %rbp
    push    %rbx
    push    %r12
    push    %r13
    
eval_loop:
    // Check slice counter
    dec     %r9
    jz      yield_to_c
    
    // Load and decode tag
    mov     (%rdi), %rax        // Load tag/fun
    test    $NODE_TAG_BIT, %al
    jz      handle_ap           // Jump if application
    test    $NODE_IND_BIT, %al
    jnz     follow_indirection
    
    // Dispatch
    shr     $2, %rax            // Extract tag
    lea     dispatch_table(%rip), %r11
    jmp     *(%r11,%rax,8)
```

### 3.2 Fast Path Operations (x86-64)

```asm
handle_ap:
    mov     %rdi, (%rsi)        // Push node
    add     $8, %rsi
    mov     (%rdi), %rdi        // n = FUN(n)
    jmp     eval_loop

; Implement combinators in primops[] frequency order
comb_B:  // B - Most frequent combinator
    ; Implementation here
    
comb_O:  // O - Second most frequent
    ; Implementation here

comb_K:  // K - Third most frequent
    sub     $16, %rsi           // POP(2)
    mov     -8(%rsi), %rdi      // TOP(-1)
    mov     (%rsi), %rax        // TOP(0)
    mov     8(%rax), %rax       // ARG(TOP(0))
    mov     %rax, 8(%rdi)       // Update node
    jmp     eval_return
```

## Phase 4: Testing and Integration (Weeks 7-8)

### 4.1 Test Strategy

#### Correctness Testing
```bash
# Run full test suite with each implementation
make runtestmhs              # C version baseline
make runtestmhs-arm64        # ARM64 optimized
make runtestmhs-x86_64       # x86-64 optimized

# Compare outputs
diff test_c.out test_arm64.out
diff test_c.out test_x86_64.out
```

#### Performance Testing
```bash
# Benchmark suite from benchmark.md
for impl in mhseval mhseval-arm64 mhseval-x86_64; do
    echo "Testing $impl:"
    time bin/$impl tests/Nfib
    bin/$impl tests/Nfib | grep "nfib/s"
done
```

### 4.2 Integration Approach

```c
// In eval.c - Minimal changes
#ifdef USE_ASM_EVAL
// Assembly version handles hot paths
value_t evali(value_t n) {
    // Setup (remains in C)
    setup_stack();
    
    // Hot loop in assembly
    n = evali_asm(n, stack_ptr);
    
    // Cleanup (remains in C)
    cleanup_stack();
    return n;
}
#else
// Original C implementation
value_t evali(value_t n) { /* existing code */ }
#endif
```

## Phase 5: Optimization and Tuning (Week 9)

### 5.1 ARM64 Specific Optimizations

```asm
// Prefetching for node traversal
handle_ap_optimized:
    str     x0, [x1], #8        // Push node
    ldr     x0, [x0]            // n = FUN(n)
    prfm    pldl1keep, [x0]     // Prefetch next node
    b       eval_loop

// Use conditional select for common patterns
comb_cond:
    cmp     x2, #0
    csel    x0, x3, x4, eq      // Select based on condition
    b       eval_loop
```

### 5.2 x86-64 Specific Optimizations

```asm
// Use CMOV for branchless code
prim_min:
    cmp     %rdx, %rcx
    cmovl   %rdx, %rcx          // Conditional move
    mov     %rcx, 8(%rdi)
    jmp     eval_return

// Align hot paths
.align 64
eval_loop:                      // Align to cache line
```

### 5.3 Performance Targets

| Operation | C Baseline | ARM64 Target | x86-64 Target |
|-----------|------------|--------------|---------------|
| nfib(37)  | 8M/s       | 40-60M/s     | 30-50M/s      |
| Sieve     | 100K/s     | 300K/s       | 250K/s        |
| Arithmetic| 50M ops/s  | 150M ops/s   | 120M ops/s    |

## Phase 6: Deployment (Week 10)

### 6.1 Build System Integration

```makefile
# Architecture detection
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_M),arm64)
    ASM_EVAL := eval_arm64.s
    ASM_FLAGS := -DUSE_ASM_EVAL
else ifeq ($(UNAME_M),aarch64)
    ASM_EVAL := eval_arm64.s
    ASM_FLAGS := -DUSE_ASM_EVAL
else ifeq ($(UNAME_M),x86_64)
    ASM_EVAL := eval_x86_64.s
    ASM_FLAGS := -DUSE_ASM_EVAL
else
    ASM_EVAL :=
    ASM_FLAGS :=
endif

# Build targets
bin/mhseval: src/runtime/eval.c $(if $(ASM_EVAL),src/runtime/$(ASM_EVAL))
    $(CC) $(ASM_FLAGS) -o $@ $^ $(CFLAGS)

# Explicit architecture builds
bin/mhseval-portable: src/runtime/eval.c
    $(CC) -o $@ $< $(CFLAGS)

bin/mhseval-arm64: src/runtime/eval.c src/runtime/eval_arm64.s
    $(CC) -DUSE_ASM_EVAL -o $@ $^ $(CFLAGS)

bin/mhseval-x86_64: src/runtime/eval.c src/runtime/eval_x86_64.s
    $(CC) -DUSE_ASM_EVAL -o $@ $^ $(CFLAGS)
```

### 6.2 Platform Selection Strategy

```c
// Runtime selection in main.c
const char* get_eval_implementation() {
#ifdef USE_ASM_EVAL
    #ifdef __aarch64__
        return "ARM64 optimized";
    #elif defined(__x86_64__)
        return "x86-64 optimized";
    #endif
#endif
    return "Portable C";
}
```

## Risk Mitigation

### Technical Risks

1. **Limited Scope Advantage**: Optimizing only `evali()` loop may not achieve full potential
   - Mitigation: Focus on hottest paths identified by profiling, inline critical helpers

2. **C/Assembly Boundary Overhead**: Function call overhead between C and assembly
   - Mitigation: Keep entire hot path in assembly, minimize transitions

3. **Platform Maintenance**: Multiple assembly implementations to maintain
   - Mitigation: Shared test suite, clear abstraction boundaries, fallback to C

4. **Register Preservation**: Must maintain C calling conventions
   - Mitigation: Clear documentation, comprehensive testing, defensive coding

### Performance Risks

1. **ARM64 vs x86-64 Differences**: Different optimization strategies needed
   - Mitigation: Architecture-specific tuning, separate optimization passes

2. **Compiler Optimization Competition**: Modern C compilers are very good
   - Mitigation: Focus on patterns C compilers struggle with (computed goto, custom calling conventions)

## Success Metrics

### Functional Requirements
- ✅ Pass all tests in `tests/` directory without modification
- ✅ Identical output to C implementation
- ✅ Maintain full compatibility with existing C runtime
- ✅ Support all platforms via C fallback
- ✅ Clean integration with existing build system

### Performance Requirements (ARM64 Primary Target)
- ✅ 5-10x speedup on nfib benchmark (40-80M nfib/s)
- ✅ 3-5x speedup on tight arithmetic loops
- ✅ 2-3x speedup on combinator-heavy code
- ✅ No performance regression for I/O bound code

### Performance Requirements (x86-64 Secondary Target)
- ✅ 3-7x speedup on nfib benchmark (25-55M nfib/s)
- ✅ 2-4x speedup on arithmetic operations
- ✅ 2x speedup on combinator dispatch

### Validation Plan

1. **Correctness Validation**:
   ```bash
   # Test all implementations
   make runtestmhs              # Portable C baseline
   make runtestmhs-arm64        # ARM64 optimized
   make runtestmhs-x86_64       # x86-64 optimized
   
   # Verify identical outputs
   for test in tests/*.hs; do
       diff <(bin/mhseval-portable $test) <(bin/mhseval-arm64 $test)
   done
   ```

2. **Performance Validation**:
   ```bash
   # Run benchmark suite
   ./benchmark_all.sh
   
   # Expected results:
   # Platform    | nfib(37) | Target  | Achieved
   # ------------|----------|---------|----------
   # C Portable  | 8M/s     | -       | baseline
   # ARM64       | 8M/s     | 40-80M/s| ?
   # x86-64      | 8M/s     | 25-55M/s| ?
   ```

3. **Platform Independence Validation**:
   ```bash
   # Verify portable build works everywhere
   make clean
   make bin/mhseval-portable
   make runtestmhs
   
   # Verify architecture detection
   make bin/mhseval  # Should auto-select optimal version
   ```

## Timeline Summary

- **Week 1**: Analysis and architecture design
- **Weeks 2-4**: ARM64 implementation (primary target)
- **Weeks 5-6**: x86-64 implementation (secondary target)
- **Weeks 7-8**: Testing and integration
- **Week 9**: Optimization and tuning
- **Week 10**: Deployment and documentation

**Total Duration**: 10 weeks (2.5 months)

### Reduced Scope Benefits
- **Faster delivery**: 10 weeks vs 16 weeks
- **Lower risk**: Only optimizing proven hot paths
- **Maintained compatibility**: Full C fallback always available
- **Incremental approach**: Can extend optimization later if needed

## Conclusion

This revised plan focuses on optimizing the critical `evali()` evaluation loop while maintaining MicroHs's commitment to portability. By providing architecture-specific optimized versions alongside the portable C implementation, we achieve:

1. **Maximum Performance**: 5-10x speedup on hot paths for ARM64 (primary) and x86-64 (secondary)
2. **Platform Independence**: Every platform still supported via C fallback
3. **Reduced Complexity**: GC, I/O, and complex operations remain in maintainable C
4. **Faster Delivery**: 10 week timeline vs original 16 weeks
5. **Lower Risk**: Narrower scope with clear C/assembly boundaries

The approach respects MicroHs's design philosophy of simplicity and portability while delivering substantial performance improvements where they matter most - in the tight evaluation loop that dominates runtime execution.