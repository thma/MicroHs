#!/bin/bash

# Simple benchmark script for comparing JIT vs non-JIT performance
# Tests compilation of a simple program multiple times

echo "=== MicroHs JIT Benchmark (Simple) ==="
echo ""

# Ensure we have a clean build
echo "Building MicroHs without JIT..."
make clean > /dev/null 2>&1
make bin/mhs > /dev/null 2>&1

# Create a moderate test program that exercises combinators
cat > test_benchmark.hs << 'EOF'
module Main where

-- Factorial function (recursive, exercises I, K, A combinators)
fact :: Int -> Int
fact 0 = 1
fact n = n * fact (n - 1)

-- Sum function with pattern matching
sumList :: [Int] -> Int
sumList [] = 0
sumList (x:xs) = x + sumList xs

-- Map-based computation
compute :: Int -> Int
compute n = sumList (map fact [1..n])

main :: IO ()
main = do
  let result = compute 10
  print result
  print (fact 20)
  print (sumList [1..100])
EOF

echo "Test program created: test_benchmark.hs"
echo ""

# Function to measure runtime using time command
run_benchmark() {
    local name=$1
    echo "=== $name ==="
    echo "Running 5 iterations..."
    
    for i in 1 2 3 4 5; do
        echo -n "  Run $i: "
        # Use time command and capture real time
        exec 2>&1
        TIME_OUTPUT=$( (time bin/mhs test_benchmark.hs > /dev/null) 2>&1 )
        # Extract real time from output (format: real 0m0.123s)
        REAL_TIME=$(echo "$TIME_OUTPUT" | grep real | awk '{print $2}' | sed 's/0m//' | sed 's/s//')
        echo "${REAL_TIME}s"
    done
    echo ""
}

# Benchmark without JIT
run_benchmark "Without JIT"

# Now rebuild with JIT enabled
echo "Rebuilding with JIT enabled (threshold=100)..."

# Create a modified eval.c with JIT enabled by default
cp src/runtime/eval.c src/runtime/eval.c.backup

# Use sed to modify the JIT settings
sed -i '' 's/int enable_jit = 0;/int enable_jit = 1;/' src/runtime/eval.c
sed -i '' 's/size_t jit_threshold = 10000;/size_t jit_threshold = 100;/' src/runtime/eval.c

# Rebuild with JIT
make clean > /dev/null 2>&1
make bin/mhs > /dev/null 2>&1

echo ""

# Benchmark with JIT
run_benchmark "With JIT (threshold=100)"

# Try with even lower threshold
echo "Rebuilding with JIT threshold=10..."
sed -i '' 's/size_t jit_threshold = 100;/size_t jit_threshold = 10;/' src/runtime/eval.c
make bin/mhs > /dev/null 2>&1

echo ""
run_benchmark "With JIT (threshold=10)"

# Restore original eval.c
echo "Restoring original files..."
mv src/runtime/eval.c.backup src/runtime/eval.c

echo ""
echo "=== Benchmark Summary ==="
echo "Manual comparison needed - look at the times above"
echo "Lower times are better"
echo ""
echo "Note: JIT compilation overhead may exceed benefits for small programs."
echo "The real benefit would come from compiling arithmetic combinators"
echo "and running longer computations."

# Clean up
rm -f test_benchmark.hs