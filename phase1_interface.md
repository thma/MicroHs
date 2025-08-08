# Phase 1 Task 4 & 5: C/Assembly Interface and Performance Framework

## Task 4: C/Assembly Calling Convention and Data Structure Alignment

### Function Interface

#### Primary Function Signature
```c
NODEPTR evali(NODEPTR n);
```
- **Input**: NODEPTR n (pointer to node to evaluate)
- **Output**: NODEPTR (pointer to evaluated node in WHNF)
- **Side effects**: Modifies global stack, may trigger GC

#### Assembly Implementation Signature
```c
// In eval.c with USE_ASM_EVAL defined
extern NODEPTR evali_asm(NODEPTR n);
```

### ARM64 Calling Convention (Primary Target)

#### Register Usage Plan
```
// Arguments and return
x0  - Input node (n), also return value

// Global state (must preserve or reload)
x19 - stack base pointer (global)
x20 - stack_ptr (global)
x21 - stack_size (global)
x22 - glob_slice (global)
x23 - cells base (heap pointer)

// Working registers (caller-saved)
x1  - Current stack pointer (cached from x20)
x2  - Temporary x (first argument)
x3  - Temporary y (second argument)  
x4  - Temporary z (third argument)
x5  - Slice counter (cached from x22)
x6  - Scratch/temporary
x7  - Tag/dispatch value
x8  - Jump table base

// Callee-saved (must preserve)
x19-x28 - Preserve across calls
x29 - Frame pointer
x30 - Link register
```

#### Stack Frame Layout
```
sp+0:   saved x29 (frame pointer)
sp+8:   saved x30 (link register)
sp+16:  saved x19
sp+24:  saved x20
...
sp+N:   local variables if needed
```

### x86-64 Calling Convention (Secondary Target)

#### Register Usage Plan
```
// Arguments and return (System V ABI)
%rdi - Input node (n), also return value

// Global state
%rbx - stack base pointer (preserved)
%r12 - stack_ptr (preserved)
%r13 - stack_size (preserved)
%r14 - glob_slice (preserved)
%r15 - cells base (preserved)

// Working registers
%rsi - Current stack pointer (cached)
%rdx - Temporary x
%rcx - Temporary y
%r8  - Temporary z
%r9  - Slice counter (cached)
%r10 - Scratch
%r11 - Jump table/indirect target
%rax - Return value prep

// Stack frame
%rbp - Frame pointer
%rsp - Stack pointer
```

### Global Variable Access

#### Critical Globals
```c
// Must be accessible from assembly
extern NODEPTR *stack;        // Stack array base
extern stackptr_t stack_ptr;  // Current stack index
extern stackptr_t stack_size; // Stack limit
extern int glob_slice;        // Reduction counter
extern node *cells;           // Heap base
```

#### Access Patterns
```asm
// ARM64
adrp    x8, stack@PAGE
ldr     x19, [x8, stack@PAGEOFF]

// x86-64  
movq    stack(%rip), %rbx
```

### Data Structure Alignment

#### Node Structure (16 bytes, naturally aligned)
```c
struct node {
  union { /* 8 bytes at offset 0 */
    struct node *uufun;
    intptr_t uuifun;
    tag_t uutag;
  } ufun;
  union { /* 8 bytes at offset 8 */
    struct node *uuarg;
    value_t uuvalue;
    /* other union members */
  } uarg;
};
```

#### Alignment Requirements
- Nodes: 16-byte aligned (natural)
- Stack: 8-byte aligned (pointer size)
- Heap: Page-aligned (4KB typical)

### C/Assembly Integration Strategy

#### Entry Wrapper
```c
#ifdef USE_ASM_EVAL
NODEPTR evali(NODEPTR n) {
    // Save C state if needed
    stackptr_t saved_sp = stack_ptr;
    
    // Call assembly version
    n = evali_asm(n);
    
    // Validate state on return
    assert(stack_ptr >= saved_sp);
    
    return n;
}
#endif
```

#### GC Integration
Assembly must call back to C for:
- Garbage collection (`gc_check()`)
- Memory allocation (`new_ap()`)
- Error handling (`ERR()`)

## Task 5: Performance Measurement Framework

### Benchmark Suite Structure

#### Core Benchmarks
```bash
# Primary benchmark (nfib)
bin/benchmark_nfib.sh:
  #!/bin/bash
  echo "=== nfib(37) Benchmark ==="
  echo "C version:"
  time bin/mhseval < /dev/null 2>&1 | grep "nfib/s"
  echo "ARM64 version:"
  time bin/mhseval-arm64 < /dev/null 2>&1 | grep "nfib/s"
  echo "Speedup: $(calculate_speedup)"

# Arithmetic benchmark
bin/benchmark_arith.sh:
  Run tests/Arith.hs with timing

# Combinator benchmark
bin/benchmark_comb.sh:
  Custom test focusing on B, K, S combinators
```

#### Makefile Targets
```makefile
# Performance testing
benchmark: benchmark-c benchmark-asm
	@echo "=== Performance Comparison ==="
	@./compare_results.sh

benchmark-c: bin/mhseval
	@echo "C Baseline:" > results_c.txt
	@for i in 1 2 3; do \
		bin/mhseval < /dev/null 2>&1 | grep "nfib/s" >> results_c.txt; \
	done

benchmark-asm: bin/mhseval-arm64
	@echo "ASM Optimized:" > results_asm.txt
	@for i in 1 2 3; do \
		bin/mhseval-arm64 < /dev/null 2>&1 | grep "nfib/s" >> results_asm.txt; \
	done

benchmark-profile: bin/mhseval bin/mhseval-arm64
	perf record -g bin/mhseval < /dev/null
	perf report > profile_c.txt
	perf record -g bin/mhseval-arm64 < /dev/null
	perf report > profile_asm.txt
```

### Performance Metrics Collection

#### Runtime Statistics
```c
// Add to eval.c for profiling
#ifdef PROFILE_ASM
typedef struct {
    uint64_t total_reductions;
    uint64_t ap_nodes;
    uint64_t indirections;
    uint64_t combinator_hits[256];
    uint64_t arithmetic_ops;
    uint64_t gc_calls;
    uint64_t cache_misses;  // Via perf_event_open
} perf_stats_t;

extern perf_stats_t perf_stats;

#define COUNT_REDUCTION(tag) perf_stats.combinator_hits[tag]++
#define COUNT_AP() perf_stats.ap_nodes++
#define COUNT_IND() perf_stats.indirections++
#endif
```

#### Assembly Instrumentation
```asm
// ARM64 performance counting
.macro COUNT_OP op
#ifdef PROFILE_ASM
    adrp    x8, perf_stats@PAGE
    add     x8, x8, perf_stats@PAGEOFF
    ldr     x9, [x8, #\op * 8]
    add     x9, x9, #1
    str     x9, [x8, #\op * 8]
#endif
.endm

handle_ap:
    COUNT_OP ap_nodes
    // ... normal code
```

### Validation Framework

#### Correctness Testing
```bash
#!/bin/bash
# validate.sh - Compare outputs between implementations

for test in tests/*.hs; do
    echo "Testing $test..."
    
    # Run with C version
    bin/mhs -itests $(basename $test .hs) && bin/mhseval > output_c.txt
    
    # Run with ASM version
    bin/mhs -itests $(basename $test .hs) && bin/mhseval-arm64 > output_asm.txt
    
    # Compare
    if ! diff -q output_c.txt output_asm.txt > /dev/null; then
        echo "FAIL: $test outputs differ"
        diff output_c.txt output_asm.txt
        exit 1
    fi
done

echo "All tests passed!"
```

#### Performance Regression Detection
```python
#!/usr/bin/env python3
# check_regression.py

import json
import sys

def load_baseline():
    with open('baseline.json') as f:
        return json.load(f)

def measure_current():
    # Run benchmarks and collect metrics
    return {
        'nfib': get_nfib_performance(),
        'arith': get_arith_performance(),
        'comb': get_comb_performance()
    }

def main():
    baseline = load_baseline()
    current = measure_current()
    
    for test, baseline_val in baseline.items():
        current_val = current[test]
        ratio = current_val / baseline_val
        
        if ratio < 0.95:  # 5% regression threshold
            print(f"REGRESSION: {test} is {ratio:.1%} of baseline")
            sys.exit(1)
        elif ratio > 1.05:
            print(f"IMPROVEMENT: {test} is {ratio:.1%} of baseline")
        else:
            print(f"STABLE: {test} within 5% of baseline")

if __name__ == '__main__':
    main()
```

### Continuous Integration

#### GitHub Actions Workflow
```yaml
name: Performance Tests

on: [push, pull_request]

jobs:
  benchmark:
    runs-on: [self-hosted, ARM64]  # Need ARM64 runner
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Build implementations
      run: |
        make clean
        make bin/mhseval
        make bin/mhseval-arm64
    
    - name: Run benchmarks
      run: |
        make benchmark > results.txt
        
    - name: Check regression
      run: |
        python3 check_regression.py
        
    - name: Upload results
      uses: actions/upload-artifact@v2
      with:
        name: benchmark-results
        path: results.txt
```

### Performance Dashboard

#### Metrics to Track
1. **Primary**: nfib/s rate
2. **Reductions/sec**: Total graph reductions
3. **Cache efficiency**: L1/L2 hit rates
4. **Branch prediction**: Misprediction rate
5. **IPC**: Instructions per cycle

#### Visualization
```html
<!-- dashboard.html -->
<div id="performance-chart">
  <canvas id="nfib-trend"></canvas>
  <script>
    // Chart showing performance over commits
    const data = loadBenchmarkHistory();
    plotTrend(data);
  </script>
</div>
```

## Summary

### Key Design Decisions

1. **Minimal Interface**: Single function `evali_asm()` minimizes integration complexity
2. **Register Allocation**: Optimized for hot path with globals in preserved registers
3. **Measurement Points**: Instrumentation at combinator level for detailed profiling
4. **Validation First**: Correctness testing before performance measurement
5. **Regression Detection**: Automated checks prevent performance degradation

### Next Steps for Phase 2

With the interface and measurement framework defined:
1. Implement basic dispatch loop in assembly
2. Add one combinator at a time with validation
3. Measure performance after each addition
4. Iterate based on profiling data

The framework ensures we can measure progress accurately and catch regressions immediately as we implement the assembly optimization.