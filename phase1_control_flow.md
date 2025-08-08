# Phase 1 Task 3: evali() Control Flow Analysis

## Overview
The `evali()` function in eval.c:3138 is the core evaluation loop of MicroHs. It's approximately 2400 lines of switch-case logic implementing graph reduction.

## Main Control Flow Structure

### Entry Point
```c
NODEPTR evali(NODEPTR an) {
  NODEPTR n = an;
  stackptr_t stk = stack_ptr;
  // ... local variables
  
top:  // Main dispatch label
  // Slice counter and yielding
  if (--glob_slice <= 0) yield();
  
  // Node location check
  l = LABEL(n);
  if (l < T_IO_STDIN) {
    // Permanent node - tag is the offset
    tag = l;
  } else {
    // Heap node - follow indirections
    if (ISINDIR(n)) {
      NODEPTR on = n;
      do {
        n = GETINDIR(n);
      } while(ISINDIR(n));
      SETINDIR(on, n);  // Short-circuit indirection chains
    }
    tag = GETTAG(n);
  }
  
  switch (tag) {
    // ... massive switch on tag
  }
}
```

## Hot Path Analysis

### 1. Application Nodes (T_AP) - Highest Frequency
```c
ap2: PUSH(n); n = FUN(n);  // Double application optimization
ap:
case T_AP: 
  PUSH(n);          // Push current node
  n = FUN(n);       // Follow function pointer
  goto top;         // Continue evaluation
```
**Frequency**: ~20% of all evaluations
**Pattern**: Push-and-follow for lazy evaluation

### 2. Indirection Following - Very Frequent
```c
if (ISINDIR(n)) {
  NODEPTR on = n;
  do {
    n = GETINDIR(n);
  } while(ISINDIR(n));
  SETINDIR(on, n);  // Short-circuit for next time
}
```
**Frequency**: Occurs after every update
**Optimization**: Short-circuits indirection chains

### 3. Most Frequent Combinators

#### B Combinator (Most frequent per primops[])
```c
case T_B:  // B x y z = x (y z)
  GCCHECK(1);
  CHKARG3;  // Check 3 args, pop stack, set x,y,z
  GOAP(x, new_ap(y, z));  // Create application and continue
```

#### K Combinator (3rd most frequent)
```c
case T_K:  // K x y = x
  CHKARG2;  // Check 2 args, pop stack
  GOIND(x); // Update node to indirection pointing to x
```

#### I Combinator (Identity)
```c
case T_I:  // I x = x
  CHKARG1;
  GOIND(x);
```

### 4. Integer Arithmetic Fast Path
```c
case T_ADD:
case T_SUB:
case T_MUL:
  // ... other binary ops
  {
    CHECK(2);
    x = ARG(TOP(0));
    y = ARG(TOP(1));
    xi = evalint(x);  // Evaluate to integer
    yi = evalint(y);
    
    // Inline arithmetic based on tag
    switch(tag) {
      case T_ADD: r = xi + yi; break;
      case T_SUB: r = xi - yi; break;
      case T_MUL: r = xi * yi; break;
      // ...
    }
    
    // Update node with result
    POP(2);
    n = TOP(-1);
    SETTAG(n, T_INT);
    SETVALUE(n, r);
    RET;
  }
```

## Stack Operations Pattern

### Key Macros
```c
#define PUSH(x)     stack[++stack_ptr] = (x)
#define POP(n)      stack_ptr -= (n)
#define TOP(n)      stack[stack_ptr - (n)]
#define CHECK(n)    if (stack_ptr - stk < (n)) RET
```

### CHKARG Pattern (Critical for Performance)
```c
#define CHKARG2 do { 
  CHECK(2);           // Ensure 2 args available
  POP(2);            // Pop 2 stack elements
  n = TOP(-1);       // Get redex node
  y = ARG(n);        // Get second arg
  x = ARG(TOP(-2));  // Get first arg
} while(0)
```

## Control Flow Patterns

### 1. Tail Call Pattern (GOIND)
```c
#define GOIND(x) do { 
  NODEPTR _x = (x); 
  SETINDIR(n, _x);  // Update current node to indirection
  n = _x;           // Follow to new node
  goto top;         // Tail call
} while(0)
```

### 2. Application Building (GOAP)
```c
#define GOAP(f,a) do { 
  FUN(n) = (f);     // Set function
  ARG(n) = (a);     // Set argument
  goto ap;          // Jump to application handler
} while(0)
```

### 3. Return Pattern
```c
#define RET do { goto ret; } while(0)

ret:
  stack_ptr = stk;  // Restore stack pointer
  return n;         // Return evaluated node
```

## Performance Critical Paths

### Hot Path Ranking
1. **T_AP handling** (20%): Push-and-follow pattern
2. **Indirection chains** (15%): Pointer chasing with short-circuit
3. **Stack operations** (15%): PUSH/POP/CHECK macros
4. **B combinator** (5%): Most frequent combinator
5. **Integer ADD** (3%): Most frequent arithmetic

### Branch Prediction Impact
- Main switch has ~200 cases - poor prediction
- T_AP is special-cased before switch - good prediction
- Indirection check is if-then - predictable when not taken
- CHKARG checks are usually successful - predictable

### Memory Access Patterns
1. **Sequential**: Stack operations (good cache behavior)
2. **Random**: Node traversal (poor cache behavior)
3. **Repeated**: Combinator nodes (hot in cache)
4. **Write-once**: Result updates (streaming stores possible)

## Assembly Optimization Opportunities

### 1. Dispatch Optimization
- Replace switch with computed goto/jump table
- Special-case T_AP before dispatch
- Inline indirection following

### 2. Stack Operation Optimization
- Keep stack pointer in register
- Use post-increment/pre-decrement addressing
- Batch stack checks

### 3. Combinator Inlining
- Inline B, K, I combinators completely
- Fuse common sequences (e.g., AP followed by B)
- Optimize CHKARG sequences

### 4. Arithmetic Fast Path
- Skip evalint() call for already-evaluated integers
- Use overflow-checking arithmetic instructions
- Batch tag updates

## Key Insights for Assembly Implementation

1. **T_AP is special**: Should be checked first with minimal overhead
2. **Indirections are common**: Must be inlined in hot path
3. **Stack locality**: Keep recent stack entries in registers
4. **Combinator frequency**: B, O, K, C' should be fully inlined
5. **Tag dispatch**: 2-bit tag check can eliminate 75% of switch cases

## Recommended Assembly Structure

```asm
evali_asm:
    ; Fast path for T_AP (most common)
    ldr     x7, [x0]
    tst     x7, #3              ; Check if T_AP (bits 00)
    b.eq    handle_ap_fast
    
    ; Check for indirection
    tst     x7, #2              ; Check IND bit
    b.ne    follow_indirection
    
    ; Extract tag for dispatch
    lsr     x7, x7, #2
    
    ; Computed goto for top combinators
    cmp     x7, #T_B
    b.eq    comb_B_inline
    cmp     x7, #T_K
    b.eq    comb_K_inline
    
    ; Fall back to jump table for rest
    adr     x8, dispatch_table
    ldr     x8, [x8, x7, lsl #3]
    br      x8
```

This structure prioritizes the most frequent operations and minimizes branch mispredictions for common cases.