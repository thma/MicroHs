/* MicroHs JIT Compiler Implementation using MIR */
#include "mhsffi.h"  /* Must be first for type definitions */
#include "jit.h"

#if WANT_JIT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>  /* For offsetof */
#include "jit_node.h"  /* Node structure definitions */

/* External variables from eval.c */
extern int verbose;
extern counter_t combinator_counts[];

/* Global JIT state */
jit_context_t *global_jit_ctx = NULL;
int enable_jit = 0;
size_t jit_threshold = 10000;  /* Default threshold */

/* Statistics */
counter_t jit_compile_count = 0;
counter_t jit_execute_count = 0;
counter_t jit_cache_hits = 0;
counter_t jit_cache_misses = 0;

/* Node tag constants from eval.c - must match exactly */
enum { 
    T_I = 16, T_K = 14, T_A = 19,
    /* Arithmetic combinators */
    T_ADD = 34, T_SUB = 35, T_MUL = 36, T_QUOT = 37, T_REM = 38,
    T_SUBR = 39, T_UQUOT = 40, T_UREM = 41, T_NEG = 42,
    /* Comparison combinators */
    T_EQ = 53, T_NE = 54, T_LT = 55, T_LE = 56, T_GT = 57, T_GE = 58,
    T_LAST_TAG = 200 
};

/* Forward declarations */
static void* compile_I_combinator(jit_context_t *ctx);
static void* compile_K_combinator(jit_context_t *ctx);
static void* compile_A_combinator(jit_context_t *ctx);
static void* compile_ADD_combinator(jit_context_t *ctx);
static void* compile_SUB_combinator(jit_context_t *ctx);
static void* compile_MUL_combinator(jit_context_t *ctx);

/* Initialize JIT system */
int jit_init(void) {
    if (global_jit_ctx) {
        return 1;  /* Already initialized */
    }
    
    global_jit_ctx = (jit_context_t*)calloc(1, sizeof(jit_context_t));
    if (!global_jit_ctx) {
        fprintf(stderr, "JIT: Failed to allocate context\n");
        return 0;
    }
    
    /* Initialize MIR context */
    global_jit_ctx->mir_ctx = MIR_init();
    if (!global_jit_ctx->mir_ctx) {
        free(global_jit_ctx);
        global_jit_ctx = NULL;
        return 0;
    }
    
    /* Create main module */
    global_jit_ctx->module = MIR_new_module(global_jit_ctx->mir_ctx, "mhs_jit");
    
    /* Allocate code cache */
    global_jit_ctx->cache_size = T_LAST_TAG + 1;
    global_jit_ctx->code_cache = (void**)calloc(global_jit_ctx->cache_size, sizeof(void*));
    if (!global_jit_ctx->code_cache) {
        MIR_finish(global_jit_ctx->mir_ctx);
        free(global_jit_ctx);
        global_jit_ctx = NULL;
        return 0;
    }
    
    global_jit_ctx->compile_threshold = jit_threshold;
    global_jit_ctx->initialized = 1;
    
    /* Always show initialization message for debugging */
    fprintf(stderr, "JIT: Initialized successfully (threshold=%zu)\n", jit_threshold);
    
    return 1;
}

/* Shutdown JIT system */
void jit_shutdown(void) {
    if (!global_jit_ctx) {
        return;
    }
    
    if (jit_compile_count > 0 || enable_jit) {  /* Show stats if JIT was used */
        fprintf(stderr, "JIT Statistics:\n");
        fprintf(stderr, "  Compilations: %llu\n", (unsigned long long)jit_compile_count);
        fprintf(stderr, "  Executions: %llu\n", (unsigned long long)jit_execute_count);
        fprintf(stderr, "  Cache hits: %llu\n", (unsigned long long)jit_cache_hits);
        fprintf(stderr, "  Cache misses: %llu\n", (unsigned long long)jit_cache_misses);
    }
    
    if (global_jit_ctx->code_cache) {
        free(global_jit_ctx->code_cache);
    }
    
    if (global_jit_ctx->mir_ctx) {
        MIR_finish(global_jit_ctx->mir_ctx);
    }
    
    free(global_jit_ctx);
    global_jit_ctx = NULL;
}

/* Check if combinator should be compiled */
int jit_should_compile(int tag) {
    if (!enable_jit || !global_jit_ctx || !global_jit_ctx->initialized) {
        return 0;
    }
    
    /* Check if already compiled */
    if (tag < global_jit_ctx->cache_size && global_jit_ctx->code_cache[tag]) {
        return 0;
    }
    
    /* Debug logging for threshold checking */
    if (verbose > 2) {
        fprintf(stderr, "JIT: Checking tag %d, count=%llu, threshold=%zu\n", 
                tag, (unsigned long long)combinator_counts[tag], 
                global_jit_ctx->compile_threshold);
    }
    
    /* Check execution count threshold */
    if (combinator_counts[tag] < global_jit_ctx->compile_threshold) {
        return 0;
    }
    
    /* Only compile supported combinators */
    switch (tag) {
    case T_I:
    case T_K:
    case T_A:
    /* Temporarily disable arithmetic combinators due to MIR issues
    case T_ADD:
    case T_SUB:
    case T_MUL:
    */
        if (verbose > 1) {
            fprintf(stderr, "JIT: Triggering compilation for tag %d\n", tag);
        }
        return 1;
    default:
        return 0;
    }
}

/* Test function: Just return the input node unchanged */
static void* compile_test_identity(jit_context_t *ctx) {
    MIR_type_t res_type = MIR_T_P;
    
    /* Create function with single pointer parameter and pointer return */
    MIR_item_t func_item = MIR_new_func(ctx->mir_ctx, "test_identity", 
                                        1, &res_type,  /* 1 return value of type pointer */
                                        1, MIR_T_P, "input"); /* 1 parameter: pointer named "input" */
    
    /* Get the input parameter register */
    MIR_reg_t input_reg = MIR_reg(ctx->mir_ctx, "input", func_item->u.func);
    
    /* Simply return the input */
    MIR_append_insn(ctx->mir_ctx, func_item,
                    MIR_new_ret_insn(ctx->mir_ctx, 1, 
                                    MIR_new_reg_op(ctx->mir_ctx, input_reg)));
    
    MIR_finish_func(ctx->mir_ctx);
    
    /* Generate code */
    void *code = MIR_gen(ctx->mir_ctx, func_item);
    
    if (verbose > 2) {
        fprintf(stderr, "JIT: Test identity function compiled\n");
    }
    
    return code;
}

/* Compile I combinator: I x = x */
static void* compile_I_combinator(jit_context_t *ctx) {
    if (verbose > 2) {
        fprintf(stderr, "JIT: Compiling I combinator\n");
    }
    
    MIR_type_t res_type = MIR_T_P;
    
    /* Create function with three parameters: node, stack, stack_ptr */
    MIR_item_t func_item = MIR_new_func(ctx->mir_ctx, "I_jit", 
                                        1, &res_type,  /* 1 return value */
                                        3,             /* 3 parameters */
                                        MIR_T_P, "node",
                                        MIR_T_P, "stack",
                                        MIR_T_I64, "stack_ptr");
    
    /* Get parameter registers - note we pass func_item->u.func */
    MIR_reg_t node_reg = MIR_reg(ctx->mir_ctx, "node", func_item->u.func);
    MIR_reg_t stack_reg = MIR_reg(ctx->mir_ctx, "stack", func_item->u.func);
    MIR_reg_t stack_ptr_reg = MIR_reg(ctx->mir_ctx, "stack_ptr", func_item->u.func);
    
    /* For I combinator, just return the node unchanged (it's an identity) */
    MIR_append_insn(ctx->mir_ctx, func_item,
                    MIR_new_ret_insn(ctx->mir_ctx, 1, 
                                    MIR_new_reg_op(ctx->mir_ctx, node_reg)));
    
    MIR_finish_func(ctx->mir_ctx);
    
    /* Generate code */
    void *code = MIR_gen(ctx->mir_ctx, func_item);
    
    if (verbose > 2) {
        fprintf(stderr, "JIT: Compiled I combinator\n");
    }
    
    return code;
}

/* Compile K combinator: K x y = x */
static void* compile_K_combinator(jit_context_t *ctx) {
    /* Debug: Starting K combinator compilation */
    if (verbose > 2) {
        fprintf(stderr, "JIT: Compiling K combinator\n");
    }
    
    MIR_type_t res_type = MIR_T_P;
    
    /* Create function */
    MIR_item_t func_item = MIR_new_func(ctx->mir_ctx, "K_jit", 
                                        1, &res_type,
                                        3,
                                        MIR_T_P, "node",
                                        MIR_T_P, "stack",
                                        MIR_T_I64, "stack_ptr");
    
    /* Get parameter registers */
    MIR_reg_t node_reg = MIR_reg(ctx->mir_ctx, "node", func_item->u.func);
    MIR_reg_t stack_reg = MIR_reg(ctx->mir_ctx, "stack", func_item->u.func);
    MIR_reg_t stack_ptr_reg = MIR_reg(ctx->mir_ctx, "stack_ptr", func_item->u.func);
    
    /* Create local registers for stack access */
    MIR_reg_t arg1_ptr = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg1_ptr");
    MIR_reg_t arg1_node = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg1_node");
    MIR_reg_t arg1_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg1_val");
    
    /* Check if we have at least 2 arguments on stack */
    MIR_label_t has_args_label = MIR_new_label(ctx->mir_ctx);
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_BGE,
            MIR_new_label_op(ctx->mir_ctx, has_args_label),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, 2)));
    
    /* Return NULL if not enough arguments */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_ret_insn(ctx->mir_ctx, 1,
            MIR_new_int_op(ctx->mir_ctx, 0)));
    
    /* has_args: */
    MIR_append_insn(ctx->mir_ctx, func_item, has_args_label);
    
    /* Calculate pointer to stack[stack_ptr-1] (second from top) */
    /* arg1_ptr = stack + (stack_ptr-1) * sizeof(NODEPTR) */
    MIR_reg_t offset = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "offset");
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_SUB,
            MIR_new_reg_op(ctx->mir_ctx, offset),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, 1)));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MUL,
            MIR_new_reg_op(ctx->mir_ctx, offset),
            MIR_new_reg_op(ctx->mir_ctx, offset),
            MIR_new_int_op(ctx->mir_ctx, sizeof(NODEPTR))));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_ADD,
            MIR_new_reg_op(ctx->mir_ctx, arg1_ptr),
            MIR_new_reg_op(ctx->mir_ctx, stack_reg),
            MIR_new_reg_op(ctx->mir_ctx, offset)));
    
    /* Load the node pointer from stack: arg1_node = *arg1_ptr */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg1_node),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_P, 0,
                          arg1_ptr, 0, 0)));
    
    /* Load the argument from node: arg1_val = arg1_node->u2.arg */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg1_val),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_P, JIT_OFFSET_ARG,
                          arg1_node, 0, 0)));
    
    /* Set indirection: node->u1.ifun = arg1_val | BIT_IND */
    MIR_reg_t ind_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "ind_val");
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_OR,
            MIR_new_reg_op(ctx->mir_ctx, ind_val),
            MIR_new_reg_op(ctx->mir_ctx, arg1_val),
            MIR_new_int_op(ctx->mir_ctx, JIT_BIT_IND)));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_FUN,
                          node_reg, 0, 0),
            MIR_new_reg_op(ctx->mir_ctx, ind_val)));
    
    /* Return the first argument */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_ret_insn(ctx->mir_ctx, 1,
            MIR_new_reg_op(ctx->mir_ctx, arg1_val)));
    
    MIR_finish_func(ctx->mir_ctx);
    
    /* Generate code */
    void *code = MIR_gen(ctx->mir_ctx, func_item);
    
    if (verbose > 2) {
        fprintf(stderr, "JIT: Compiled K combinator\n");
    }
    
    return code;
}

/* Compile A combinator: A x y = y */
static void* compile_A_combinator(jit_context_t *ctx) {
    /* Debug: Starting A combinator compilation */
    if (verbose > 2) {
        fprintf(stderr, "JIT: Compiling A combinator\n");
    }
    
    MIR_type_t res_type = MIR_T_P;
    
    /* Create function */
    MIR_item_t func_item = MIR_new_func(ctx->mir_ctx, "A_jit", 
                                        1, &res_type,
                                        3,
                                        MIR_T_P, "node",
                                        MIR_T_P, "stack",
                                        MIR_T_I64, "stack_ptr");
    
    /* Get parameter registers */
    MIR_reg_t node_reg = MIR_reg(ctx->mir_ctx, "node", func_item->u.func);
    MIR_reg_t stack_reg = MIR_reg(ctx->mir_ctx, "stack", func_item->u.func);
    MIR_reg_t stack_ptr_reg = MIR_reg(ctx->mir_ctx, "stack_ptr", func_item->u.func);
    
    /* Create local registers for stack access */
    MIR_reg_t arg2_ptr = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg2_ptr");
    MIR_reg_t arg2_node = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg2_node");
    MIR_reg_t arg2_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg2_val");
    
    /* Check if we have at least 2 arguments on stack */
    MIR_label_t has_args_label = MIR_new_label(ctx->mir_ctx);
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_BGE,
            MIR_new_label_op(ctx->mir_ctx, has_args_label),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, 2)));
    
    /* Return NULL if not enough arguments */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_ret_insn(ctx->mir_ctx, 1,
            MIR_new_int_op(ctx->mir_ctx, 0)));
    
    /* has_args: */
    MIR_append_insn(ctx->mir_ctx, func_item, has_args_label);
    
    /* Calculate pointer to stack[stack_ptr] (top element) */
    /* arg2_ptr = stack + stack_ptr * sizeof(NODEPTR) */
    MIR_reg_t offset = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "offset");
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MUL,
            MIR_new_reg_op(ctx->mir_ctx, offset),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, sizeof(NODEPTR))));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_ADD,
            MIR_new_reg_op(ctx->mir_ctx, arg2_ptr),
            MIR_new_reg_op(ctx->mir_ctx, stack_reg),
            MIR_new_reg_op(ctx->mir_ctx, offset)));
    
    /* Load the node pointer from stack: arg2_node = *arg2_ptr */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg2_node),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_P, 0,
                          arg2_ptr, 0, 0)));
    
    /* Load the argument from node: arg2_val = arg2_node->u2.arg */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg2_val),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_P, JIT_OFFSET_ARG,
                          arg2_node, 0, 0)));
    
    /* Set indirection: node->u1.ifun = arg2_val | BIT_IND */
    MIR_reg_t ind_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "ind_val");
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_OR,
            MIR_new_reg_op(ctx->mir_ctx, ind_val),
            MIR_new_reg_op(ctx->mir_ctx, arg2_val),
            MIR_new_int_op(ctx->mir_ctx, JIT_BIT_IND)));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_FUN,
                          node_reg, 0, 0),
            MIR_new_reg_op(ctx->mir_ctx, ind_val)));
    
    /* Return the second argument */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_ret_insn(ctx->mir_ctx, 1,
            MIR_new_reg_op(ctx->mir_ctx, arg2_val)));
    
    MIR_finish_func(ctx->mir_ctx);
    
    /* Generate code */
    void *code = MIR_gen(ctx->mir_ctx, func_item);
    
    if (verbose > 2) {
        fprintf(stderr, "JIT: Compiled A combinator\n");
    }
    
    return code;
}

/* Compile ADD combinator: ADD x y = x + y (both must be integers) */
static void* compile_ADD_combinator(jit_context_t *ctx) {
    if (verbose > 2) {
        fprintf(stderr, "JIT: Compiling ADD combinator\n");
    }
    
    MIR_type_t res_type = MIR_T_P;
    
    /* Create function */
    MIR_item_t func_item = MIR_new_func(ctx->mir_ctx, "ADD_jit", 
                                        1, &res_type,
                                        3,
                                        MIR_T_P, "node",
                                        MIR_T_P, "stack",
                                        MIR_T_I64, "stack_ptr");
    
    /* Get parameter registers */
    MIR_reg_t node_reg = MIR_reg(ctx->mir_ctx, "node", func_item->u.func);
    MIR_reg_t stack_reg = MIR_reg(ctx->mir_ctx, "stack", func_item->u.func);
    MIR_reg_t stack_ptr_reg = MIR_reg(ctx->mir_ctx, "stack_ptr", func_item->u.func);
    
    /* Create local registers */
    MIR_reg_t arg1_ptr = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg1_ptr");
    MIR_reg_t arg1_node = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg1_node");
    MIR_reg_t arg1_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg1_val");
    MIR_reg_t arg2_ptr = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg2_ptr");
    MIR_reg_t arg2_node = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg2_node");
    MIR_reg_t arg2_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg2_val");
    MIR_reg_t result_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "result_val");
    
    /* Check if we have at least 2 arguments on stack */
    MIR_label_t has_args_label = MIR_new_label(ctx->mir_ctx);
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_BGE,
            MIR_new_label_op(ctx->mir_ctx, has_args_label),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, 2)));
    
    /* Return NULL if not enough arguments */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_ret_insn(ctx->mir_ctx, 1,
            MIR_new_int_op(ctx->mir_ctx, 0)));
    
    /* has_args: */
    MIR_append_insn(ctx->mir_ctx, func_item, has_args_label);
    
    /* Load first argument from stack[stack_ptr-1] */
    MIR_reg_t offset1 = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "offset1");
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_SUB,
            MIR_new_reg_op(ctx->mir_ctx, offset1),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, 1)));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MUL,
            MIR_new_reg_op(ctx->mir_ctx, offset1),
            MIR_new_reg_op(ctx->mir_ctx, offset1),
            MIR_new_int_op(ctx->mir_ctx, sizeof(NODEPTR))));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_ADD,
            MIR_new_reg_op(ctx->mir_ctx, arg1_ptr),
            MIR_new_reg_op(ctx->mir_ctx, stack_reg),
            MIR_new_reg_op(ctx->mir_ctx, offset1)));
    
    /* Load the node pointer and get its integer value */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg1_node),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_P, 0,
                          arg1_ptr, 0, 0)));
    
    /* Load integer value from arg1_node->u2.arg (offset 8) */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg1_val),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_ARG,
                          arg1_node, 0, 0)));
    
    /* Load second argument from stack[stack_ptr] */
    MIR_reg_t offset2 = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "offset2");
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MUL,
            MIR_new_reg_op(ctx->mir_ctx, offset2),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, sizeof(NODEPTR))));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_ADD,
            MIR_new_reg_op(ctx->mir_ctx, arg2_ptr),
            MIR_new_reg_op(ctx->mir_ctx, stack_reg),
            MIR_new_reg_op(ctx->mir_ctx, offset2)));
    
    /* Load the node pointer and get its integer value */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg2_node),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_P, 0,
                          arg2_ptr, 0, 0)));
    
    /* Load integer value from arg2_node->u2.arg (offset 8) */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg2_val),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_ARG,
                          arg2_node, 0, 0)));
    
    /* Perform addition */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_ADD,
            MIR_new_reg_op(ctx->mir_ctx, result_val),
            MIR_new_reg_op(ctx->mir_ctx, arg1_val),
            MIR_new_reg_op(ctx->mir_ctx, arg2_val)));
    
    /* Store result in node->u2.val */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_ARG,
                          node_reg, 0, 0),
            MIR_new_reg_op(ctx->mir_ctx, result_val)));
    
    /* Set node tag to T_INT (value 3) with correct bit pattern */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_FUN,
                          node_reg, 0, 0),
            MIR_new_int_op(ctx->mir_ctx, 3))); /* T_INT = 3 */
    
    /* Return the node */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_ret_insn(ctx->mir_ctx, 1,
            MIR_new_reg_op(ctx->mir_ctx, node_reg)));
    
    MIR_finish_func(ctx->mir_ctx);
    
    /* Generate code */
    void *code = MIR_gen(ctx->mir_ctx, func_item);
    
    if (verbose > 2) {
        fprintf(stderr, "JIT: Compiled ADD combinator\n");
    }
    
    return code;
}

/* Compile SUB combinator: SUB x y = x - y (both must be integers) */
static void* compile_SUB_combinator(jit_context_t *ctx) {
    if (verbose > 2) {
        fprintf(stderr, "JIT: Compiling SUB combinator\n");
    }
    
    MIR_type_t res_type = MIR_T_P;
    
    /* Create function */
    MIR_item_t func_item = MIR_new_func(ctx->mir_ctx, "SUB_jit", 
                                        1, &res_type,
                                        3,
                                        MIR_T_P, "node",
                                        MIR_T_P, "stack",
                                        MIR_T_I64, "stack_ptr");
    
    /* Get parameter registers */
    MIR_reg_t node_reg = MIR_reg(ctx->mir_ctx, "node", func_item->u.func);
    MIR_reg_t stack_reg = MIR_reg(ctx->mir_ctx, "stack", func_item->u.func);
    MIR_reg_t stack_ptr_reg = MIR_reg(ctx->mir_ctx, "stack_ptr", func_item->u.func);
    
    /* Create local registers */
    MIR_reg_t arg1_ptr = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg1_ptr");
    MIR_reg_t arg1_node = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg1_node");
    MIR_reg_t arg1_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg1_val");
    MIR_reg_t arg2_ptr = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg2_ptr");
    MIR_reg_t arg2_node = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg2_node");
    MIR_reg_t arg2_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg2_val");
    MIR_reg_t result_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "result_val");
    
    /* Check if we have at least 2 arguments on stack */
    MIR_label_t has_args_label = MIR_new_label(ctx->mir_ctx);
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_BGE,
            MIR_new_label_op(ctx->mir_ctx, has_args_label),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, 2)));
    
    /* Return NULL if not enough arguments */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_ret_insn(ctx->mir_ctx, 1,
            MIR_new_int_op(ctx->mir_ctx, 0)));
    
    /* has_args: */
    MIR_append_insn(ctx->mir_ctx, func_item, has_args_label);
    
    /* Load first argument from stack[stack_ptr-1] */
    MIR_reg_t offset1 = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "offset1");
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_SUB,
            MIR_new_reg_op(ctx->mir_ctx, offset1),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, 1)));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MUL,
            MIR_new_reg_op(ctx->mir_ctx, offset1),
            MIR_new_reg_op(ctx->mir_ctx, offset1),
            MIR_new_int_op(ctx->mir_ctx, sizeof(NODEPTR))));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_ADD,
            MIR_new_reg_op(ctx->mir_ctx, arg1_ptr),
            MIR_new_reg_op(ctx->mir_ctx, stack_reg),
            MIR_new_reg_op(ctx->mir_ctx, offset1)));
    
    /* Load the node pointer and get its integer value */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg1_node),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_P, 0,
                          arg1_ptr, 0, 0)));
    
    /* Load integer value from arg1_node->u2.arg (offset 8) */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg1_val),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_ARG,
                          arg1_node, 0, 0)));
    
    /* Load second argument from stack[stack_ptr] */
    MIR_reg_t offset2 = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "offset2");
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MUL,
            MIR_new_reg_op(ctx->mir_ctx, offset2),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, sizeof(NODEPTR))));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_ADD,
            MIR_new_reg_op(ctx->mir_ctx, arg2_ptr),
            MIR_new_reg_op(ctx->mir_ctx, stack_reg),
            MIR_new_reg_op(ctx->mir_ctx, offset2)));
    
    /* Load the node pointer and get its integer value */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg2_node),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_P, 0,
                          arg2_ptr, 0, 0)));
    
    /* Load integer value from arg2_node->u2.arg (offset 8) */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg2_val),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_ARG,
                          arg2_node, 0, 0)));
    
    /* Perform subtraction */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_SUB,
            MIR_new_reg_op(ctx->mir_ctx, result_val),
            MIR_new_reg_op(ctx->mir_ctx, arg1_val),
            MIR_new_reg_op(ctx->mir_ctx, arg2_val)));
    
    /* Store result in node->u2.val */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_ARG,
                          node_reg, 0, 0),
            MIR_new_reg_op(ctx->mir_ctx, result_val)));
    
    /* Set node tag to T_INT (value 3) with correct bit pattern */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_FUN,
                          node_reg, 0, 0),
            MIR_new_int_op(ctx->mir_ctx, 3))); /* T_INT = 3 */
    
    /* Return the node */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_ret_insn(ctx->mir_ctx, 1,
            MIR_new_reg_op(ctx->mir_ctx, node_reg)));
    
    MIR_finish_func(ctx->mir_ctx);
    
    /* Generate code */
    void *code = MIR_gen(ctx->mir_ctx, func_item);
    
    if (verbose > 2) {
        fprintf(stderr, "JIT: Compiled SUB combinator\n");
    }
    
    return code;
}

/* Compile MUL combinator: MUL x y = x * y (both must be integers) */
static void* compile_MUL_combinator(jit_context_t *ctx) {
    if (verbose > 2) {
        fprintf(stderr, "JIT: Compiling MUL combinator\n");
    }
    
    MIR_type_t res_type = MIR_T_P;
    
    /* Create function */
    MIR_item_t func_item = MIR_new_func(ctx->mir_ctx, "MUL_jit", 
                                        1, &res_type,
                                        3,
                                        MIR_T_P, "node",
                                        MIR_T_P, "stack",
                                        MIR_T_I64, "stack_ptr");
    
    /* Get parameter registers */
    MIR_reg_t node_reg = MIR_reg(ctx->mir_ctx, "node", func_item->u.func);
    MIR_reg_t stack_reg = MIR_reg(ctx->mir_ctx, "stack", func_item->u.func);
    MIR_reg_t stack_ptr_reg = MIR_reg(ctx->mir_ctx, "stack_ptr", func_item->u.func);
    
    /* Create local registers */
    MIR_reg_t arg1_ptr = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg1_ptr");
    MIR_reg_t arg1_node = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg1_node");
    MIR_reg_t arg1_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg1_val");
    MIR_reg_t arg2_ptr = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg2_ptr");
    MIR_reg_t arg2_node = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg2_node");
    MIR_reg_t arg2_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "arg2_val");
    MIR_reg_t result_val = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "result_val");
    
    /* Check if we have at least 2 arguments on stack */
    MIR_label_t has_args_label = MIR_new_label(ctx->mir_ctx);
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_BGE,
            MIR_new_label_op(ctx->mir_ctx, has_args_label),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, 2)));
    
    /* Return NULL if not enough arguments */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_ret_insn(ctx->mir_ctx, 1,
            MIR_new_int_op(ctx->mir_ctx, 0)));
    
    /* has_args: */
    MIR_append_insn(ctx->mir_ctx, func_item, has_args_label);
    
    /* Load first argument from stack[stack_ptr-1] */
    MIR_reg_t offset1 = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "offset1");
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_SUB,
            MIR_new_reg_op(ctx->mir_ctx, offset1),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, 1)));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MUL,
            MIR_new_reg_op(ctx->mir_ctx, offset1),
            MIR_new_reg_op(ctx->mir_ctx, offset1),
            MIR_new_int_op(ctx->mir_ctx, sizeof(NODEPTR))));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_ADD,
            MIR_new_reg_op(ctx->mir_ctx, arg1_ptr),
            MIR_new_reg_op(ctx->mir_ctx, stack_reg),
            MIR_new_reg_op(ctx->mir_ctx, offset1)));
    
    /* Load the node pointer and get its integer value */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg1_node),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_P, 0,
                          arg1_ptr, 0, 0)));
    
    /* Load integer value from arg1_node->u2.arg (offset 8) */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg1_val),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_ARG,
                          arg1_node, 0, 0)));
    
    /* Load second argument from stack[stack_ptr] */
    MIR_reg_t offset2 = MIR_new_func_reg(ctx->mir_ctx, func_item->u.func, MIR_T_I64, "offset2");
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MUL,
            MIR_new_reg_op(ctx->mir_ctx, offset2),
            MIR_new_reg_op(ctx->mir_ctx, stack_ptr_reg),
            MIR_new_int_op(ctx->mir_ctx, sizeof(NODEPTR))));
    
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_ADD,
            MIR_new_reg_op(ctx->mir_ctx, arg2_ptr),
            MIR_new_reg_op(ctx->mir_ctx, stack_reg),
            MIR_new_reg_op(ctx->mir_ctx, offset2)));
    
    /* Load the node pointer and get its integer value */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg2_node),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_P, 0,
                          arg2_ptr, 0, 0)));
    
    /* Load integer value from arg2_node->u2.arg (offset 8) */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_reg_op(ctx->mir_ctx, arg2_val),
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_ARG,
                          arg2_node, 0, 0)));
    
    /* Perform multiplication */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MUL,
            MIR_new_reg_op(ctx->mir_ctx, result_val),
            MIR_new_reg_op(ctx->mir_ctx, arg1_val),
            MIR_new_reg_op(ctx->mir_ctx, arg2_val)));
    
    /* Store result in node->u2.val */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_ARG,
                          node_reg, 0, 0),
            MIR_new_reg_op(ctx->mir_ctx, result_val)));
    
    /* Set node tag to T_INT (value 3) with correct bit pattern */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_insn(ctx->mir_ctx, MIR_MOV,
            MIR_new_mem_op(ctx->mir_ctx, MIR_T_I64, JIT_OFFSET_FUN,
                          node_reg, 0, 0),
            MIR_new_int_op(ctx->mir_ctx, 3))); /* T_INT = 3 */
    
    /* Return the node */
    MIR_append_insn(ctx->mir_ctx, func_item,
        MIR_new_ret_insn(ctx->mir_ctx, 1,
            MIR_new_reg_op(ctx->mir_ctx, node_reg)));
    
    MIR_finish_func(ctx->mir_ctx);
    
    /* Generate code */
    void *code = MIR_gen(ctx->mir_ctx, func_item);
    
    if (verbose > 2) {
        fprintf(stderr, "JIT: Compiled MUL combinator\n");
    }
    
    return code;
}

/* Compile a combinator to native code */
void* jit_compile_combinator(int tag) {
    if (!global_jit_ctx || !global_jit_ctx->initialized) {
        return NULL;
    }
    
    /* Check cache first */
    if (tag < global_jit_ctx->cache_size && global_jit_ctx->code_cache[tag]) {
        jit_cache_hits++;
        return global_jit_ctx->code_cache[tag];
    }
    
    jit_cache_misses++;
    
    void* code = NULL;
    
    /* Compile based on combinator type */
    switch (tag) {
    case T_I:
        code = compile_I_combinator(global_jit_ctx);
        break;
    case T_K:
        code = compile_K_combinator(global_jit_ctx);
        break;
    case T_A:
        code = compile_A_combinator(global_jit_ctx);
        break;
    /* Temporarily disable arithmetic combinators due to MIR issues
    case T_ADD:
        code = compile_ADD_combinator(global_jit_ctx);
        break;
    case T_SUB:
        code = compile_SUB_combinator(global_jit_ctx);
        break;
    case T_MUL:
        code = compile_MUL_combinator(global_jit_ctx);
        break;
    */
    default:
        return NULL;
    }
    
    /* Cache the compiled code */
    if (code && tag < global_jit_ctx->cache_size) {
        global_jit_ctx->code_cache[tag] = code;
        global_jit_ctx->cache_used++;
        jit_compile_count++;
    }
    
    return code;
}

/* Execute JIT compiled code */
NODEPTR jit_execute(NODEPTR n, void* jit_code, NODEPTR* stack, intptr_t stack_ptr) {
    if (!jit_code) {
        return NULL;  /* Fallback to interpreter */
    }
    
    jit_execute_count++;
    
    /* Cast to function pointer and call with stack info */
    jit_func_t func = (jit_func_t)jit_code;
    NODEPTR result = func(n, stack, stack_ptr);
    
    return result;
}

/* Clear JIT code cache */
void jit_clear_cache(void) {
    if (!global_jit_ctx || !global_jit_ctx->code_cache) {
        return;
    }
    
    memset(global_jit_ctx->code_cache, 0, global_jit_ctx->cache_size * sizeof(void*));
    global_jit_ctx->cache_used = 0;
    
    if (verbose > 1) {
        fprintf(stderr, "JIT: Code cache cleared\n");
    }
}

#endif /* WANT_JIT */