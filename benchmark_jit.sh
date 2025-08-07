#!/bin/bash

# Benchmark script for comparing JIT vs non-JIT performance
# Tests self-compilation of MicroHs

echo "=== MicroHs JIT Benchmark ==="
echo "Testing self-compilation performance"
echo ""

# Ensure we have a clean build
make clean > /dev/null 2>&1
make bin/mhs > /dev/null 2>&1

# Create a test runner that compiles Main.hs
cat > test_selfcompile.sh << 'EOF'
#!/bin/bash
export MHSDIR=.
bin/mhs -oselftest src/MicroHs/Main.hs
EOF
chmod +x test_selfcompile.sh

echo "Warming up..."
# Warm up run (non-JIT) to ensure files are cached
./test_selfcompile.sh > /dev/null 2>&1

echo ""
echo "=== Benchmark 1: Without JIT ==="
echo "Running 3 iterations..."

# Run without JIT (3 times and average)
total_time=0
for i in 1 2 3; do
    echo -n "  Run $i: "
    start=$(date +%s.%N)
    ./test_selfcompile.sh > /dev/null 2>&1
    end=$(date +%s.%N)
    runtime=$(echo "$end - $start" | bc)
    echo "${runtime}s"
    total_time=$(echo "$total_time + $runtime" | bc)
done

avg_nojit=$(echo "scale=3; $total_time / 3" | bc)
echo "Average without JIT: ${avg_nojit}s"
echo ""

# Now rebuild with JIT enabled and lower threshold
echo "=== Rebuilding with JIT enabled ==="
make clean > /dev/null 2>&1

# Create version with JIT flags enabled by default
cat > src/runtime/jit_config.h << 'EOF'
#ifndef JIT_CONFIG_H
#define JIT_CONFIG_H

/* Default JIT configuration for benchmarking */
#define DEFAULT_JIT_ENABLED 1
#define DEFAULT_JIT_THRESHOLD 1000

#endif
EOF

# Modify eval.c to use default JIT settings
cp src/runtime/eval.c src/runtime/eval.c.backup
sed -i '' '
/int enable_jit = 0;/c\
int enable_jit = DEFAULT_JIT_ENABLED;
/size_t jit_threshold = 10000;/c\
size_t jit_threshold = DEFAULT_JIT_THRESHOLD;
' src/runtime/eval.c

# Add include for config
sed -i '' '/#include "eval.h"/a\
#ifdef WANT_JIT\
#include "jit_config.h"\
#endif
' src/runtime/eval.c

make bin/mhs > /dev/null 2>&1

echo "Warming up with JIT..."
# Warm up run with JIT
./test_selfcompile.sh > /dev/null 2>&1

echo ""
echo "=== Benchmark 2: With JIT (threshold=1000) ==="
echo "Running 3 iterations..."

# Run with JIT (3 times and average)
total_time=0
for i in 1 2 3; do
    echo -n "  Run $i: "
    start=$(date +%s.%N)
    ./test_selfcompile.sh > /dev/null 2>&1
    end=$(date +%s.%N)
    runtime=$(echo "$end - $start" | bc)
    echo "${runtime}s"
    total_time=$(echo "$total_time + $runtime" | bc)
done

avg_jit=$(echo "scale=3; $total_time / 3" | bc)
echo "Average with JIT: ${avg_jit}s"

# Calculate improvement
if (( $(echo "$avg_jit < $avg_nojit" | bc -l) )); then
    improvement=$(echo "scale=1; (($avg_nojit - $avg_jit) / $avg_nojit) * 100" | bc)
    speedup=$(echo "scale=2; $avg_nojit / $avg_jit" | bc)
    echo ""
    echo "=== Results ==="
    echo "Without JIT: ${avg_nojit}s"
    echo "With JIT:    ${avg_jit}s"
    echo "Improvement: ${improvement}%"
    echo "Speedup:     ${speedup}x"
else
    slowdown=$(echo "scale=1; (($avg_jit - $avg_nojit) / $avg_nojit) * 100" | bc)
    echo ""
    echo "=== Results ==="
    echo "Without JIT: ${avg_nojit}s"
    echo "With JIT:    ${avg_jit}s"
    echo "Slowdown:    ${slowdown}% (JIT overhead exceeds benefit)"
fi

# Restore original eval.c
mv src/runtime/eval.c.backup src/runtime/eval.c
rm -f src/runtime/jit_config.h

echo ""
echo "Benchmark complete!"