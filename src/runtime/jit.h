/* MicroHs JIT Compiler Module using MIR */
#ifndef MHS_JIT_H
#define MHS_JIT_H

/* This header requires mhsffi.h to be included first by the caller */
/* It's included by eval.c which already has all necessary headers */

#if WANT_JIT

#include "mir/mir.h"
#include "mir/mir-gen.h"

/* Forward declarations for types from eval.c */
typedef uintptr_t counter_t;

/* JIT context structure */
typedef struct jit_context {
    MIR_context_t mir_ctx;
    MIR_module_t module;
    int initialized;
    size_t compile_threshold;
    void **code_cache;  /* Maps node_tag to compiled function */
    size_t cache_size;
    size_t cache_used;
} jit_context_t;

/* JIT function signature for compiled combinators */
typedef NODEPTR (*jit_func_t)(NODEPTR node, NODEPTR* stack, intptr_t stack_ptr);

/* Global JIT context */
extern jit_context_t *global_jit_ctx;
extern int enable_jit;
extern size_t jit_threshold;

/* JIT API - using int for node_tag to avoid redefinition */
int jit_init(void);
void jit_shutdown(void);
int jit_should_compile(int tag);
void* jit_compile_combinator(int tag);
NODEPTR jit_execute(NODEPTR n, void* jit_code, NODEPTR* stack, intptr_t stack_ptr);
void jit_clear_cache(void);

/* Statistics */
extern counter_t jit_compile_count;
extern counter_t jit_execute_count;
extern counter_t jit_cache_hits;
extern counter_t jit_cache_misses;

#endif /* WANT_JIT */

#endif /* MHS_JIT_H */