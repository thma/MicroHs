/* Node structure definitions for JIT compilation */
#ifndef JIT_NODE_H
#define JIT_NODE_H

#include <stddef.h>
#include <stdint.h>

/* Node structure layout from eval.c */
/* This must match exactly with the definitions in eval.c */

/* For 64-bit systems */
#if INTPTR_MAX == 0x7fffffffffffffff

/* The node structure is two pointers/values */
struct jit_node {
    union {
        struct jit_node *fun;    /* Function pointer for AP nodes */
        intptr_t         ifun;   /* Integer representation */
        uintptr_t        tag;    /* Tag with bits */
    } u1;
    union {
        struct jit_node *arg;    /* Argument pointer for AP nodes */
        intptr_t         value;  /* Integer value */
        void            *ptr;    /* Generic pointer */
    } u2;
};

/* Field access offsets */
#define JIT_NODE_SIZE     16        /* 2 * 8 bytes on 64-bit */
#define JIT_OFFSET_FUN    0         /* First word: function/tag */
#define JIT_OFFSET_ARG    8         /* Second word: argument/value */

#else
/* 32-bit systems */

struct jit_node {
    union {
        struct jit_node *fun;
        intptr_t         ifun;
        uintptr_t        tag;
    } u1;
    union {
        struct jit_node *arg;
        intptr_t         value;
        void            *ptr;
    } u2;
};

#define JIT_NODE_SIZE     8         /* 2 * 4 bytes on 32-bit */
#define JIT_OFFSET_FUN    0         /* First word: function/tag */
#define JIT_OFFSET_ARG    4         /* Second word: argument/value */

#endif

/* Tag bits from eval.c */
#define JIT_BIT_TAG   1
#define JIT_BIT_IND   2
#define JIT_BIT_NOTAP (JIT_BIT_TAG | JIT_BIT_IND)
#define JIT_TAG_SHIFT 2

/* Helper macros for JIT code generation */
#define JIT_IS_AP(tag_val)  (!((tag_val) & JIT_BIT_NOTAP))
#define JIT_IS_IND(tag_val) ((tag_val) & JIT_BIT_IND)
#define JIT_GET_TAG(tag_val) ((tag_val) >> JIT_TAG_SHIFT)

#endif /* JIT_NODE_H */