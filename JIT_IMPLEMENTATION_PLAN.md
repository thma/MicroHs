# JIT Compiler Integration Plan for MicroHs Runtime

Based on comprehensive analysis of the MicroHs runtime system, this document outlines a detailed implementation plan for integrating a JIT compiler into `bin/mhseval`.

## Current Architecture Analysis

**Runtime System (`src/runtime/eval.c`)**:
- Pure combinator-based evaluation using large `switch` statement in `evali()`
- Mark-scan garbage collector with bitmap-based allocation (`free_map`)
- Graph reduction with indirection nodes for sharing
- 80+ combinator types (S, K, I, B, C, arithmetic, etc.)
- Memory layout: 2-word cells with tag + payload or function/argument pointers
- Stack-based evaluation with continuation-passing style

**Key Performance Bottlenecks**:
- Switch dispatch overhead for combinator interpretation 
- Frequent allocation/GC pressure from intermediate nodes
- Indirection chasing overhead
- No locality optimization for related computations

## JIT Implementation Strategy

### Phase 1: Profiling and Hot Path Detection

**1.1 Runtime Profiling Infrastructure**
- Add execution counters to each combinator case in `evali()` 
- Track combinator sequence frequency patterns
- Measure evaluation depth and stack usage patterns
- Identify hot combinators and common reduction sequences

**1.2 Profiling Workload Generation**
- Use the **benchmarking suite workloads** (self-compilation + test suite) to generate realistic profiling data
- Run profiling instrumented interpreter on representative workloads to collect:
  - Combinator execution frequency histograms
  - Common combinator sequence patterns (bigrams, trigrams)
  - Argument strictness analysis across different program phases
  - Memory allocation patterns and GC pressure points
- Create profiling dataset that represents real MicroHs usage patterns

**1.3 Hot Path Detection**
- Implement threshold-based compilation triggers based on profiling data analysis
- Track combinator chains that execute together frequently in benchmark workloads
- Detect recursive patterns in combinator applications from real programs
- Use statistical analysis of benchmark runs to identify optimization opportunities
- Validate hot path detection against both self-compilation and test suite execution patterns

### Phase 2: Lightweight JIT Backend

**2.1 JIT Compiler Selection**
- **Recommended**: MIR (Medium Internal Representation) - MIT licensed, lightweight
- **Alternative 1**: Direct machine code generation for x86-64/ARM64
- **Alternative 2**: GNU Lightning (LGPL v3, dynamic linking required)
- **Not recommended**: LLVM (too heavyweight for MicroHs philosophy)

**2.2 Code Generation Strategy**
- Generate native code for hot combinator sequences
- Implement direct-threaded evaluation (jump tables) for warm paths
- Create specialized entry points for common combinator arities
- Maintain interpreter fallback for cold paths and unsupported patterns

### Phase 3: JIT Integration Points

**3.1 Evaluation Loop Modification**
```c
NODEPTR evali(NODEPTR n) {
  // Check if node has compiled version
  if (HAS_JIT_CODE(n)) {
    return call_jit_function(n, JIT_CODE(n));
  }
  
  // Hot path detection
  INCREMENT_EVAL_COUNTER(n);
  if (SHOULD_COMPILE(n)) {
    compile_hot_path(n);
  }
  
  // Existing interpreter fallback
  switch (tag) { ... }
}
```

**3.2 Memory Layout Extensions**
- Add JIT code pointer field to node structure (conditional compilation)
- Extend node tags to include JIT status flags
- Create JIT code cache with GC integration
- Implement code invalidation for garbage collected nodes

**3.3 GC Integration**
- Mark JIT code pages as roots during garbage collection
- Implement code cache eviction policy (LRU or frequency-based)
- Handle node relocation for JIT compiled functions
- Maintain mapping between nodes and generated code

### Phase 4: Optimization Strategies

**4.1 Combinator-Specific Optimizations**
- **Identity/Projection**: Compile away I, K, A combinators entirely
- **Composition**: Fuse B combinator chains into single native functions
- **Arithmetic**: Generate inline machine instructions for T_ADD, T_MUL, etc.
- **Comparison**: Optimize branching for T_EQ, T_LT chains

**4.2 Graph Reduction Optimizations**
- Eliminate indirection chasing in compiled code
- Implement lazy evaluation with native continuations
- Generate specialized code for known argument patterns
- Optimize common combinator application sequences

**4.3 Memory Management Optimizations**
- Reduce allocation pressure through escape analysis
- Implement stack allocation for short-lived nodes
- Generate GC-aware native code with safe points
- Optimize memory layout for better cache locality

### Phase 5: Performance Monitoring and Fallback

**5.1 Performance Metrics**
- Track JIT compilation time vs. execution speedup
- Monitor code cache hit rates and eviction frequency
- Measure memory overhead of JIT infrastructure
- Compare performance against pure interpreter baseline

**5.2 Adaptive Compilation**
- Implement tiered compilation (fast JIT → optimized JIT)
- Dynamic deoptimization for mis-speculated paths
- Runtime profile feedback for better optimization decisions
- Automatic fallback to interpreter for problematic patterns

**5.3 Debugging and Diagnostics**
- Maintain mapping between native code and combinator source
- Implement JIT code disassembly for debugging
- Add runtime switches to disable JIT for debugging
- Performance profiling hooks for optimization analysis

## Implementation Roadmap

### Milestone 1: Infrastructure (2-3 weeks)
- Add profiling counters to existing evaluator
- Implement hot path detection mechanisms  
- Create JIT backend abstraction layer
- Extend build system for JIT dependencies

### Milestone 2: Basic JIT (3-4 weeks)
- Implement simple combinator compilation (I, K, A)
- Create code cache management
- Integrate with existing memory allocator
- Basic GC integration for code pages

### Milestone 3: Advanced Optimizations (4-5 weeks)
- Compile arithmetic and comparison operations
- Implement combinator fusion optimizations
- Add adaptive compilation thresholds
- Performance tuning and profiling analysis

### Milestone 4: Production Ready (2-3 weeks)
- Error handling and edge case coverage
- Comprehensive testing across all combinator types
- Documentation and configuration options
- Integration with existing build/test infrastructure

## Technical Considerations

**Portability Requirements**:
- Must work across all MicroHs targets (unix, windows, emscripten)
- Graceful degradation when JIT unavailable
- Minimal impact on binary size for embedded targets
- Optional compilation to maintain existing compatibility

**Memory Constraints**:
- Code cache size limits for embedded systems
- GC integration without breaking existing semantics  
- Stack overflow prevention in deeply nested JIT calls
- Memory fragmentation mitigation strategies

**Compatibility Preservation**:
- Maintain exact same semantics as interpreter
- Preserve exception handling behavior
- Support for all existing combinator types
- Backward compatibility with serialized expressions

## Expected Performance Gains

- **Combinator dispatch**: 3-5x improvement via eliminated switch overhead
- **Arithmetic operations**: 5-10x improvement via native instruction generation
- **Common patterns**: 2-4x improvement via fusion and specialization
- **Overall throughput**: 2-3x improvement for compute-intensive workloads

## Build System Integration

The JIT integration should be optional and configurable via:
- `make` variables (e.g., `WANT_JIT=1`)
- Runtime flags (`+RTS -jit -RTS`)
- Compile-time feature detection for JIT backend availability
- Graceful fallback when JIT libraries unavailable

## Testing Strategy

### Functional Testing
- Comprehensive regression testing against existing test suite
- Cross-platform testing (x86-64, ARM64, emscripten)
- Memory usage and GC stress testing
- JIT vs. interpreter correctness validation

### Performance Benchmarking

**Representative Workloads**:
1. **Self-compilation benchmark**: Compile the MicroHs compiler sources (`src/MicroHs/*.hs`) using MicroHs itself
   - Measures real-world combinator evaluation patterns
   - Tests sustained performance on large, complex programs
   - Includes type checking, desugaring, and code generation phases

2. **Test suite execution**: Run all functional tests from `tests/` directory
   - Covers diverse language features and edge cases
   - Tests both quick computations and longer-running algorithms
   - Validates correctness across all combinator types

**Benchmark Protocol**:
- Run each benchmark **10 times** for statistical significance
- Report comprehensive timing statistics:
  - **Median time** (primary metric for robust comparison)
  - **Standard deviation** (measures consistency)
  - **Minimum time** (best-case performance)
  - **Maximum time** (worst-case performance)
  - **95th percentile** (typical worst-case)
- Measure both JIT and interpreter modes for direct comparison
- Record memory usage and GC statistics alongside timing
- Test with different heap sizes (`-H` flag) to assess scalability

**Benchmark Implementation**:
```bash
# Self-compilation benchmark
for i in {1..10}; do
  time bin/mhs +RTS -jit -RTS $(MHSINC) $(MAINMODULE) -ogenerated/mhs-jit.c
done

# Test suite benchmark  
for i in {1..10}; do
  time make -C tests runtestmhs MHS="../bin/mhs +RTS -jit -RTS"
done
```

**Performance Metrics Tracking**:
- Baseline interpreter performance (no JIT)
- JIT with different compilation thresholds
- Memory overhead of JIT infrastructure
- Code cache hit/miss rates
- JIT compilation time vs. execution speedup ratio

**Integration with Phase 1 Profiling**:
- The same benchmark workloads will be used in Phase 1 to generate profiling data for hot path detection
- Profiling runs will use identical self-compilation and test suite workloads to ensure JIT optimizations target real usage patterns
- Benchmark statistics will validate that hot path detection accurately identifies performance-critical code sections
- Performance improvements measured in benchmarks will directly correlate with profiling-guided JIT compilation decisions

---

The JIT integration would provide substantial performance improvements while maintaining MicroHs's excellent portability and minimal dependency profile. The implementation can be done incrementally with careful attention to maintaining the existing system's reliability and cross-platform compatibility.

## Implementation Notes

This plan prioritizes:
1. **Incremental development** - Each phase builds on the previous
2. **Compatibility preservation** - Existing functionality remains unchanged
3. **Portability** - Works across all MicroHs target platforms
4. **Performance** - Focus on high-impact optimizations first
5. **Maintainability** - Clean abstractions and optional compilation