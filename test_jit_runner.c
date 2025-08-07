#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test wrapper that sets JIT flags before calling main
// Compile this into the main executable with special flags

int original_main(int argc, char **argv);

// Override the main function
#define main original_main
#include "src/runtime/main.c"
#undef main

// External variables
extern int verbose;
extern int enable_jit;
extern size_t jit_threshold;

int main(int argc, char **argv) {
    // Set JIT flags before calling original main
    fprintf(stderr, "JIT Test Wrapper: Setting JIT flags\n");
    verbose = 3;
    enable_jit = 1;
    jit_threshold = 5;
    
    // Pass through to original main
    return original_main(argc, argv);
}