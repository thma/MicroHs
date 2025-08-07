#!/bin/bash

echo "=== MicroHs nfib JIT Benchmark (Proper) ==="
echo ""

# First, build WITHOUT JIT
echo "Building MicroHs WITHOUT JIT..."
make clean > /dev/null 2>&1
make bin/mhs WANT_JIT=0 > /dev/null 2>&1

# Compile nfib
echo "Compiling nfib.hs without JIT..."
MHSDIR=. bin/mhs -onfib_nojit nfib.hs > /dev/null 2>&1

echo ""
echo "=== Running nfib WITHOUT JIT ==="
echo "Running 5 iterations..."

times_nojit=()
for i in 1 2 3 4 5; do
    echo -n "  Run $i: "
    start=$(perl -MTime::HiRes=time -e 'print time')
    MHSDIR=. ./nfib_nojit > /dev/null 2>&1
    end=$(perl -MTime::HiRes=time -e 'print time')
    runtime=$(perl -e "print $end - $start")
    echo "${runtime}s"
    times_nojit+=($runtime)
done

# Calculate average
avg_nojit=$(perl -e "print ($(IFS=+; echo "${times_nojit[*]}")) / ${#times_nojit[@]}")
echo "Average without JIT: ${avg_nojit}s"

# Now build WITH JIT
echo ""
echo "Building MicroHs WITH JIT..."

# First, modify jit.c to enable JIT by default with low threshold
cp src/runtime/jit.c src/runtime/jit.c.backup
sed -i.bak 's/int enable_jit = 0;/int enable_jit = 1;/' src/runtime/jit.c
sed -i.bak 's/size_t jit_threshold = 10000;/size_t jit_threshold = 50;/' src/runtime/jit.c

# Build with JIT support
make clean > /dev/null 2>&1
make bin/mhs WANT_JIT=1 > /dev/null 2>&1

# Compile nfib with JIT-enabled runtime
echo "Compiling nfib.hs with JIT..."
MHSDIR=. bin/mhs -onfib_jit nfib.hs > /dev/null 2>&1

echo ""
echo "=== Running nfib WITH JIT (threshold=50) ==="
echo "Running 5 iterations..."

times_jit=()
for i in 1 2 3 4 5; do
    echo -n "  Run $i: "
    start=$(perl -MTime::HiRes=time -e 'print time')
    MHSDIR=. ./nfib_jit > /dev/null 2>&1
    end=$(perl -MTime::HiRes=time -e 'print time')
    runtime=$(perl -e "print $end - $start")
    echo "${runtime}s"
    times_jit+=($runtime)
done

# Calculate average
avg_jit=$(perl -e "print ($(IFS=+; echo "${times_jit[*]}")) / ${#times_jit[@]}")
echo "Average with JIT: ${avg_jit}s"

# Calculate improvement
echo ""
echo "=== Results Summary ==="
echo "Without JIT: ${avg_nojit}s"
echo "With JIT:    ${avg_jit}s"

improvement=$(perl -e "print (($avg_nojit - $avg_jit) / $avg_nojit) * 100")
speedup=$(perl -e "print $avg_nojit / $avg_jit")

echo "Improvement: ${improvement}%"
echo "Speedup:     ${speedup}x"

# Restore original jit.c
mv src/runtime/jit.c.backup src/runtime/jit.c
rm -f src/runtime/jit.c.bak

echo ""
echo "Benchmark complete!"