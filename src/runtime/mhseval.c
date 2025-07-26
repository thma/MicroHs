#include "mhseval.h"
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Provide stub implementations for missing symbols before including eval.c

// eval.c expects: extern uint8_t *combexpr;
uint8_t *combexpr = NULL;

// eval.c expects: extern int combexprlen;
int combexprlen = 0;

// Include the original eval.c implementation
// This will include mhsffi.h and declare the FFI tables
#include "eval.c"

// Now define the FFI tables after the structs are declared
struct ffi_entry *xffi_table = NULL;
struct ffe_entry *xffe_table = NULL;

struct MhsContext {
    jmp_buf error_jmp;
    char error_msg[1024];
    int initialized;
    int error_occurred;
};

// Global context pointer for error handling
static struct MhsContext* current_ctx = NULL;

MhsContextPtr mhs_init_context(void) {
    struct MhsContext* ctx = malloc(sizeof(struct MhsContext));
    if (!ctx) return NULL;
    
    memset(ctx, 0, sizeof(struct MhsContext));
    ctx->initialized = 0;
    ctx->error_occurred = 0;
    strcpy(ctx->error_msg, "No error");
    
    // Set up error handling for initialization
    current_ctx = ctx;
    if (setjmp(ctx->error_jmp) != 0) {
        // Error during initialization
        current_ctx = NULL;
        return ctx; // Return context with error state
    }
    
    // Initialize the MicroHs runtime (similar to mhs_init)
    heap_size = HEAP_CELLS;
    stack_size = STACK_SIZE;
    
    init_nodes();
    stack = malloc(sizeof(NODEPTR) * stack_size);
    if (!stack) {
        strcpy(ctx->error_msg, "Failed to allocate stack");
        ctx->error_occurred = 1;
        current_ctx = NULL;
        return ctx;
    }
    
    CLEARSTK();
    init_stableptr();
    
    ctx->initialized = 1;
    current_ctx = NULL;
    return ctx;
}

void mhs_free_context(MhsContextPtr ctx) {
    if (!ctx) return;
    
    // Cleanup MicroHs runtime resources
    if (ctx->initialized) {
        if (stack) {
            free(stack);
            stack = NULL;
        }
        if (cells) {
            free(cells);
            cells = NULL;
        }
        if (free_map) {
            free(free_map);
            free_map = NULL;
        }
        if (sp_table) {
            free(sp_table);
            sp_table = NULL;
        }
    }
    
    free(ctx);
}

// Convert a MicroHs node to a C string representation
static char* node_to_string(NODEPTR node, size_t* len) {
    // Open a write buffer
    BFILE *bf = openb_wr_buf();
    if (!bf) return NULL;
    
    // Print the node to the buffer (without header)
    printb(bf, node, 0);
    
    // Get the buffer contents - fix type mismatch
    uint8_t* result_ptr;
    size_t result_size;
    get_buf(bf, &result_ptr, &result_size);
    
    // Allocate a new buffer and copy the data
    char* result = malloc(result_size + 1);
    if (result) {
        memcpy(result, result_ptr, result_size);
        result[result_size] = '\0';
        *len = result_size;
    }
    
    closeb(bf);
    return result;
}

int mhs_eval_string(MhsContextPtr ctx, const char* expr, size_t len, char** result, size_t* result_len) {
    if (!ctx) return -1;
    if (!ctx->initialized) return -1;
    if (ctx->error_occurred) return -1;
    
    // Set up error handling
    current_ctx = ctx;
    ctx->error_occurred = 0;
    
    if (setjmp(ctx->error_jmp) != 0) {
        // Error occurred during evaluation
        current_ctx = NULL;
        return -1;
    }
    
    // Create a read buffer from the input string
    BFILE *bf = openb_rd_buf((uint8_t*)expr, len);
    if (!bf) {
        strcpy(ctx->error_msg, "Failed to create input buffer");
        ctx->error_occurred = 1;
        current_ctx = NULL;
        return -1;
    }
    
    // Parse the expression
    NODEPTR prog;
    prog = parse_top(bf, 0);
    // int c = getb(bf);
    // if (c == 'v') {
    //     // Put back the version character and parse as a complete program
    //     ungetb(c, bf);
    //     prog = parse_top(bf, 0);
    // } else {
    //     // Put back the character and try to parse as a single expression
    //     ungetb(c, bf);
    //     prog = parse(bf);
    // }
    
    closeb(bf);
    
    if (!prog) {
        fprintf(stderr, "Failed to parse expression: %s\n", ctx->error_msg);
        strcpy(ctx->error_msg, "Failed to parse expression");
        ctx->error_occurred = 1;
        current_ctx = NULL;
        return -1;
    }
    
    // Clear the stack and evaluate the expression
    //CLEARSTK();
    
    // Evaluate the parsed program
    start_exec(prog);
    gc();                     
    pp(stdout, prog);
    NODEPTR eval_result = prog;
    //NODEPTR eval_result = evali(prog);
    
    // Convert result to string
    *result = node_to_string(eval_result, result_len);
    if (!*result) {
        strcpy(ctx->error_msg, "Failed to convert result to string");
        ctx->error_occurred = 1;
        current_ctx = NULL;
        return -1;
    }
    
    current_ctx = NULL;
    return 0; // Success
}

void mhs_free_result(char* result) {
    if (result) {
        free(result);
    }
}

const char* mhs_get_error(MhsContextPtr ctx) {
    if (!ctx) return "Invalid context";
    return ctx->error_msg;
}