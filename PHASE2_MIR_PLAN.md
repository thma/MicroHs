# Phase 2 Implementation Plan: MIR JIT Backend Integration

## Overview

This document details the implementation plan for integrating MIR (Medium Internal Representation) as the JIT backend for MicroHs runtime. MIR is chosen for its MIT license (perfect Apache 2.0 compatibility), lightweight design, and good optimization capabilities.

## Why MIR?

### License Compatibility ✅
- **MIT License** - Fully compatible with Apache 2.0
- No linking restrictions or relicensing requirements
- Can be statically linked without concerns

### Technical Advantages
- **Lightweight**: ~500KB code size, minimal dependencies
- **Portable**: Supports x86-64, AArch64, PPC64, s390x, RISC-V
- **Optimizing**: Built-in optimizations (dead code elimination, constant folding, etc.)
- **C-friendly**: Designed to work well with C code
- **Active development**: Maintained by Vladimir Makarov (GCC developer)

## Implementation Steps

### Step 1: MIR Integration (Week 1)

#### 1.1 Add MIR as Git Submodule
```bash
git submodule add https://github.com/vnmakarov/mir.git src/runtime/mir
```

#### 1.2 Update Build System
```makefile
# In Makefile
MIR_SRC = src/runtime/mir/mir.c src/runtime/mir/mir-gen.c
MIR_FLAGS = -DMIR_PARALLEL_GEN=0 -DMIR_NO_SCAN=1
CCFLAGS += $(MIR_FLAGS) -I src/runtime/mir

# Conditional compilation
ifeq ($(WANT_JIT),1)
  CCSRC += $(MIR_SRC) src/runtime/jit.c
  CCFLAGS += -DWANT_JIT=1
endif
```

#### 1.3 Create JIT Module Structure
```c
// src/runtime/jit.h
#ifndef MHS_JIT_H
#define MHS_JIT_H

#include "eval.h"
#include "mir.h"
#include "mir-gen.h"

typedef struct jit_context {
    MIR_context_t mir_ctx;
    MIR_module_t module;
    int initialized;
    size_t compile_threshold;
    void **code_cache;  // Maps node_tag to compiled function
} jit_context_t;

// JIT API
int jit_init(void);
void jit_shutdown(void);
int jit_should_compile(enum node_tag tag);
void* jit_compile_combinator(enum node_tag tag);
NODEPTR jit_execute(NODEPTR n, void* jit_code);

#endif
```

### Step 2: Basic Combinator Compilation (Week 1-2)

#### 2.1 Simple Combinators First
Start with combinators that profiling identified as hot and are simple to compile:

```c
// src/runtime/jit.c
static void* compile_I_combinator(jit_context_t *ctx) {
    // MIR code for I combinator: I x = x
    MIR_item_t func_item;
    MIR_type_t res_type = MIR_T_P;  // Pointer type for NODEPTR
    
    func_item = MIR_new_func(ctx->mir_ctx, "I_jit", 1, &res_type, 1, 
                             MIR_T_P, "x");
    MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_P, "result");
    
    // Simply return the argument
    MIR_append_insn(ctx->mir_ctx, func_item->u.func,
                    MIR_new_insn(ctx->mir_ctx, MIR_MOV,
                                MIR_new_reg_op(ctx->mir_ctx, "result"),
                                MIR_new_reg_op(ctx->mir_ctx, "x")));
    MIR_append_insn(ctx->mir_ctx, func_item->u.func,
                    MIR_new_ret_insn(ctx->mir_ctx, 1,
                                    MIR_new_reg_op(ctx->mir_ctx, "result")));
    
    MIR_finish_func(ctx->mir_ctx);
    MIR_load_module(ctx->mir_ctx, ctx->module);
    return MIR_gen(ctx->mir_ctx, func_item);
}
```

#### 2.2 Arithmetic Combinators
Compile arithmetic operations to native instructions:

```c
static void* compile_ADD_combinator(jit_context_t *ctx) {
    // Generate MIR for: ADD x y = mkInt(evalint(x) + evalint(y))
    // This involves:
    // 1. Call evalint on both arguments
    // 2. Add the results
    // 3. Call mkInt to box the result
}
```

### Step 3: Integration with evali() (Week 2)

#### 3.1 Modify Evaluation Loop
```c
// In eval.c, modify evali() function
NODEPTR evali(NODEPTR an) {
    // ... existing code ...
    
#if WANT_JIT
    /* Check for JIT compiled version */
    if (enable_jit && combinator_counts[tag] > jit_threshold) {
        void* jit_code = jit_code_cache[tag];
        if (!jit_code) {
            jit_code = jit_compile_combinator(tag);
            jit_code_cache[tag] = jit_code;
        }
        if (jit_code) {
            return jit_execute(n, jit_code);
        }
    }
#endif
    
    /* Record profiling data */
    record_combinator(tag);
    
    switch (tag) {
    // ... existing cases ...
    }
}
```

#### 3.2 JIT Execution Wrapper
```c
NODEPTR jit_execute(NODEPTR n, void* jit_code) {
    typedef NODEPTR (*jit_func_t)(NODEPTR*, stackptr_t);
    jit_func_t func = (jit_func_t)jit_code;
    
    // Pass current stack and node
    NODEPTR result = func(&stack[stack_ptr], n);
    
    // Update stack pointer if needed
    // Handle any exceptions or special cases
    
    return result;
}
```

### Step 4: Combinator Fusion (Week 2-3)

#### 4.1 Pattern Recognition
Based on profiling bigrams, implement fusion for common patterns:

```c
static void* compile_AP_AP_fusion(jit_context_t *ctx) {
    // Compile the AP->AP pattern as a single operation
    // This eliminates intermediate allocations
}

static void* compile_AP_B_fusion(jit_context_t *ctx) {
    // Fuse AP->B pattern (very common per profiling)
}
```

#### 4.2 Fusion Detection
```c
static int detect_fusion_opportunity(enum node_tag tag1, enum node_tag tag2) {
    // Check bigram table for high-frequency patterns
    if (tag1 == T_AP && tag2 == T_AP) return FUSION_AP_AP;
    if (tag1 == T_AP && tag2 == T_B) return FUSION_AP_B;
    // ... more patterns
    return NO_FUSION;
}
```

### Step 5: Memory Management & GC Integration (Week 3)

#### 5.1 GC-Safe Points
```c
// Ensure JIT code has GC safe points
static void emit_gc_check(MIR_context_t ctx, size_t alloc_count) {
    // Generate code to check if GC is needed
    // If yes, save registers and call gc()
}
```

#### 5.2 Code Cache Management
```c
typedef struct code_cache_entry {
    enum node_tag tag;
    void* code;
    size_t exec_count;
    struct code_cache_entry* next;
} code_cache_entry_t;

static void evict_cold_code(void) {
    // LRU eviction when cache is full
}
```

### Step 6: Testing & Benchmarking (Week 3-4)

#### 6.1 Correctness Testing
- Run full test suite with JIT enabled
- Compare results with interpreter-only mode
- Stress test with concurrent evaluation

#### 6.2 Performance Benchmarking
```bash
# Benchmark targets
make timecompile-jit      # Self-compilation with JIT
make runtestmhs-jit       # Test suite with JIT
make nfibtest-jit         # Fibonacci with JIT

# Compare with baseline
make benchmark-compare    # Show speedup metrics
```

## Configuration & Runtime Flags

### Build Configuration
```makefile
# Enable JIT compilation (default: off)
WANT_JIT ?= 0

# JIT threshold (executions before compilation)
JIT_THRESHOLD ?= 1000

# Maximum code cache size (bytes)
JIT_CACHE_SIZE ?= 10485760  # 10MB
```

### Runtime Flags
```bash
# Enable JIT at runtime
bin/mhs +RTS -jit -RTS ...

# Set compilation threshold
bin/mhs +RTS -jit-threshold=5000 -RTS ...

# Enable JIT profiling
bin/mhs +RTS -jit -profile -RTS ...
```

## Expected Challenges & Solutions

### Challenge 1: Stack Management
**Issue**: MIR needs to interact with MicroHs's stack
**Solution**: Pass stack pointer to JIT functions, maintain stack discipline

### Challenge 2: Exception Handling
**Issue**: JIT code must handle MicroHs exceptions
**Solution**: Generate exception check points, use setjmp/longjmp integration

### Challenge 3: Indirection Nodes
**Issue**: Must handle T_IND nodes correctly
**Solution**: Generate indir() calls in JIT code, maintain sharing semantics

### Challenge 4: Code Size
**Issue**: JIT code cache could grow large
**Solution**: Implement eviction policy, compress cold code

## Success Metrics

### Performance Goals
- **Target**: 2-3x speedup on self-compilation
- **Minimum**: 1.5x speedup to justify complexity
- **Stretch**: 4x speedup on arithmetic-heavy code

### Quality Metrics
- Zero test failures with JIT enabled
- Code cache hit rate > 90%
- JIT compilation time < 5% of execution time
- Memory overhead < 20% of base heap size

## Implementation Timeline

### Week 1: Foundation
- [ ] Integrate MIR library
- [ ] Basic JIT infrastructure
- [ ] Compile I, K, A combinators

### Week 2: Core Combinators  
- [ ] Compile arithmetic operations
- [ ] Integrate with evali()
- [ ] Basic fusion patterns

### Week 3: Optimization
- [ ] Advanced fusion patterns
- [ ] GC integration
- [ ] Code cache management

### Week 4: Testing & Tuning
- [ ] Full test suite validation
- [ ] Performance benchmarking
- [ ] Documentation
- [ ] Performance tuning

## Next Steps

1. **Obtain MIR**: Clone and study MIR examples
2. **Prototype**: Create standalone MIR test for combinator compilation
3. **Integrate**: Add MIR to MicroHs build system
4. **Implement**: Follow weekly plan above
5. **Benchmark**: Validate performance improvements

This plan provides a concrete path to implementing JIT compilation using MIR, with perfect license compatibility and alignment with MicroHs's minimal dependency philosophy.