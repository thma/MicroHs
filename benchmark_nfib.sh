#!/bin/bash

echo "=== MicroHs nfib JIT Benchmark ==="
echo ""

# First, build without JIT (default)
echo "Building MicroHs without JIT..."
make clean > /dev/null 2>&1
make bin/mhs > /dev/null 2>&1

# Compile nfib
echo "Compiling nfib.hs..."
MHSDIR=. bin/mhs -onfib_nojit nfib.hs > /dev/null 2>&1

echo ""
echo "=== Running nfib WITHOUT JIT ==="
echo "Running 3 iterations..."

total_time=0
for i in 1 2 3; do
    echo -n "  Run $i: "
    start=$(date +%s.%N 2>/dev/null || date +%s)
    MHSDIR=. ./nfib_nojit > /dev/null 2>&1
    end=$(date +%s.%N 2>/dev/null || date +%s)
    
    # Calculate runtime (works on both Linux and macOS)
    if command -v bc > /dev/null; then
        runtime=$(echo "$end - $start" | bc)
    else
        runtime=$((end - start))
    fi
    echo "${runtime}s"
    
    if command -v bc > /dev/null; then
        total_time=$(echo "$total_time + $runtime" | bc)
    else
        total_time=$((total_time + runtime))
    fi
done

if command -v bc > /dev/null; then
    avg_nojit=$(echo "scale=3; $total_time / 3" | bc)
else
    avg_nojit=$((total_time / 3))
fi
echo "Average without JIT: ${avg_nojit}s"

# Now build with JIT enabled
echo ""
echo "Building MicroHs with JIT enabled..."

# Modify jit.c to enable JIT with low threshold
cp src/runtime/jit.c src/runtime/jit.c.backup
sed -i.bak 's/int enable_jit = 0;/int enable_jit = 1;/' src/runtime/jit.c
sed -i.bak 's/size_t jit_threshold = 10000;/size_t jit_threshold = 100;/' src/runtime/jit.c

# Rebuild
make clean > /dev/null 2>&1
make bin/mhs > /dev/null 2>&1

# Compile nfib with JIT-enabled runtime
echo "Compiling nfib.hs with JIT-enabled runtime..."
MHSDIR=. bin/mhs -onfib_jit nfib.hs > /dev/null 2>&1

echo ""
echo "=== Running nfib WITH JIT (threshold=100) ==="
echo "Running 3 iterations..."

total_time=0
for i in 1 2 3; do
    echo -n "  Run $i: "
    start=$(date +%s.%N 2>/dev/null || date +%s)
    MHSDIR=. ./nfib_jit > /dev/null 2>&1
    end=$(date +%s.%N 2>/dev/null || date +%s)
    
    # Calculate runtime
    if command -v bc > /dev/null; then
        runtime=$(echo "$end - $start" | bc)
    else
        runtime=$((end - start))
    fi
    echo "${runtime}s"
    
    if command -v bc > /dev/null; then
        total_time=$(echo "$total_time + $runtime" | bc)
    else
        total_time=$((total_time + runtime))
    fi
done

if command -v bc > /dev/null; then
    avg_jit=$(echo "scale=3; $total_time / 3" | bc)
else
    avg_jit=$((total_time / 3))
fi
echo "Average with JIT: ${avg_jit}s"

# Calculate improvement
echo ""
echo "=== Results Summary ==="
echo "Without JIT: ${avg_nojit}s"
echo "With JIT:    ${avg_jit}s"

if command -v bc > /dev/null; then
    if (( $(echo "$avg_jit < $avg_nojit" | bc -l) )); then
        improvement=$(echo "scale=1; (($avg_nojit - $avg_jit) / $avg_nojit) * 100" | bc)
        speedup=$(echo "scale=2; $avg_nojit / $avg_jit" | bc)
        echo "Improvement: ${improvement}%"
        echo "Speedup:     ${speedup}x"
    else
        slowdown=$(echo "scale=1; (($avg_jit - $avg_nojit) / $avg_nojit) * 100" | bc)
        echo "Slowdown:    ${slowdown}% (JIT overhead exceeds benefit)"
    fi
fi

# Restore original jit.c
mv src/runtime/jit.c.backup src/runtime/jit.c
rm -f src/runtime/jit.c.bak

echo ""
echo "Benchmark complete!"