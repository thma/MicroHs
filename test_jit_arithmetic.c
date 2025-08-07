#include <stdio.h>
#include <stdlib.h>

/* External functions from MicroHs runtime */
extern int enable_jit;
extern size_t jit_threshold;
extern int jit_init(void);
extern void jit_shutdown(void);
extern void MhsStart(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    /* Enable JIT with low threshold */
    enable_jit = 1;
    jit_threshold = 2;  /* Very low threshold for testing */
    
    printf("JIT Test: Running arithmetic test with JIT enabled\n");
    printf("JIT Test: Threshold set to %zu\n", jit_threshold);
    
    /* Initialize JIT */
    if (!jit_init()) {
        fprintf(stderr, "Failed to initialize JIT\n");
        return 1;
    }
    
    /* Run the arithmetic test */
    char *test_args[] = {"test_jit_arithmetic", "test_arithmetic.comb"};
    MhsStart(2, test_args);
    
    /* Shutdown JIT */
    jit_shutdown();
    
    return 0;
}