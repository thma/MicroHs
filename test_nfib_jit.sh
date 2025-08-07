#!/bin/bash

# Enable JIT with very low threshold
cp src/runtime/jit.c src/runtime/jit.c.backup
cat > src/runtime/jit.c.patch << 'EOF'
--- a/src/runtime/jit.c
+++ b/src/runtime/jit.c
@@ -17,8 +17,8 @@
 /* Global JIT state */
 jit_context_t *global_jit_ctx = NULL;
-int enable_jit = 0;
-size_t jit_threshold = 10000;  /* Default threshold */
+int enable_jit = 1;
+size_t jit_threshold = 10;  /* Very low threshold for testing */
 
 /* Statistics */
 counter_t jit_compile_count = 0;
@@ -70,7 +70,7 @@
     global_jit_ctx->compile_threshold = jit_threshold;
     global_jit_ctx->initialized = 1;
     
-    if (verbose > 0) {
+    if (1) {  /* Always show JIT init */
         fprintf(stderr, "JIT: Initialized with threshold %zu\n", jit_threshold);
     }
     
@@ -131,7 +131,7 @@
     
     /* Only compile supported combinators */
     switch (tag) {
-    if (verbose > 1) {
+    if (1) {  /* Always show compilation trigger */
         fprintf(stderr, "JIT: Triggering compilation for tag %d\n", tag);
     }
EOF

patch src/runtime/jit.c < src/runtime/jit.c.patch

# Rebuild
echo "Building with JIT enabled and verbose..."
make clean > /dev/null 2>&1
make bin/mhs > /dev/null 2>&1

# Compile nfib
MHSDIR=. bin/mhs -onfib_test nfib.hs > /dev/null 2>&1

echo "Running nfib with JIT (should show JIT activity)..."
MHSDIR=. ./nfib_test 2>&1 | head -100

# Restore
mv src/runtime/jit.c.backup src/runtime/jit.c
rm -f src/runtime/jit.c.patch