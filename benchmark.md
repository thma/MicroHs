# MicroHs Performance Benchmarking Guide

This document describes the performance benchmarking capabilities and tools available in the MicroHs compiler project.

## Quick Start

### Basic Benchmarks
- `make nfibtest` - Run the nfib(37) benchmark (naive Fibonacci)
- `make timecompile` - Time compilation of the compiler itself
- `make timecachecompile` - Time compilation with cache enabled
- `bin/mhs -T Module` - Enable profiling statistics during compilation
- Runtime with `-T` flag: `./program +RTS -T -RTS` - Show runtime profiling stats

## Compilation Performance

### Timing Compilation Speed
The Makefile provides several targets to measure compilation performance:

```bash
# Time MicroHs compiling itself
make timecompile           # Using bin/mhs with verbose output
make timemhscompile        # Using installed mhs
make timegmhscompile      # Using GHC-built mhs  
make timeghccompile       # Direct GHC compilation

# With compilation cache
make timecachecompile      # Build cache then compile with it
```

All timing targets display the current date and git commit hash for reproducibility.

### Compilation Statistics
Use the `-s` flag to show compilation speed in lines/second:
```bash
bin/mhs -s Module
```

Use `-v` (verbose) to see per-module timing breakdown:
```bash
bin/mhs -v Module
# Shows: "importing done MicroHs.Exp, 284ms (91 + 193)"
# Format: total_time (parse_time + typecheck_time)
```

## Runtime Performance

### The nfib Benchmark
The classic naive Fibonacci benchmark (`tests/Nfib.hs`) is used to measure runtime performance:

```haskell
nfib :: Int -> Int
nfib n = case n < 2 of
  False -> nfib (n - 1) + nfib (n - 2) + 1
  True  -> 1
```

Running: `make nfibtest`

The benchmark calculates nfib(37) and reports:
- Result: 126491971
- Time taken in milliseconds
- Performance in thousands of nfib operations per second

Typical performance comparison:
- MicroHs: ~8M nfib/s
- GHC -O2: ~535M nfib/s (approximately 60-70x faster)

### Runtime Profiling
Enable profiling statistics with the `-T` flag during both compilation and runtime:

```bash
# Compile with profiling
bin/mhs -T Module -oProgram

# Run with profiling
./Program +RTS -T -RTS
```

This generates dynamic function usage statistics showing which combinators are executed most frequently.

### Runtime Statistics
The runtime provides an `IO.stats` primitive (`lib/Primitives.hs`) that returns:
- Number of allocations (Word)
- Number of reductions since last GC slice (Word)

Access via: `primStats :: IO (Word, Word)`

### Time Measurement Utilities
- `System.IO.TimeMilli` - Get current time in milliseconds
- `System.CPUTime` - Get CPU time with microsecond precision

## Test Suite Performance

### Running Test Suites
```bash
make runtest              # Using GHC-compiled mhs
make runtestmhs          # Using MicroHs-compiled mhs
make runtestemscripten  # Using JavaScript target
make everytest           # All tests including benchmarks
make everytestmhs       # All tests with MicroHs-compiled mhs
```

### Example Programs
The repository includes several test programs useful for benchmarking:

- `Example.hs` - Simple factorial calculation
- `tests/Nfib.hs` - Naive Fibonacci benchmark
- `tests/Sieve.hs` - Prime number sieve using lazy lists
- `tests/Arith.hs`, `tests/Arith64.hs` - Integer arithmetic operations
- `tests/DArith.hs` - Double precision arithmetic
- `tests/FArith.hs` - Float arithmetic

Run individual examples:
```bash
bin/mhs -r Example        # Compile and run immediately
bin/mhs Example && bin/mhseval  # Compile then run with evaluator
bin/mhs Example -oEx && ./Ex    # Compile to native executable
```

### Test Suite Structure
Tests in `tests/Makefile` compare output against reference files:
- Each test has a `.hs` source file
- Expected output in `.ref` file
- Tests run via `bin/mhseval` (combinator evaluator)

## Memory and GC Performance

### Runtime Flags for Performance Tuning
```bash
./program +RTS -H50M -K100k -v -RTS
```
- `-H SIZE` - Set heap size (default 50M cells, each cell ~16 bytes)
- `-K SIZE` - Set stack size (default 100k entries)
- `-v` - Verbose output including GC statistics
- `-B` - Ring bell on every GC (for debugging)

Size suffixes: `k` (thousand), `M` (million), `G` (billion)

### Memory Statistics
With `-v` flag, the runtime reports:
- GC count and timing
- Heap usage and cell allocations
- Number of reductions performed

## Compilation Cache Performance

The compilation cache significantly speeds up incremental builds:

```bash
make cachetest            # Test cache functionality
make cachelib            # Build cache for standard library
```

Cache operations:
- `-CW` - Write compilation cache to `.mhscache`
- `-CR` - Read compilation cache from `.mhscache`
- `-C` - Both read and write cache

The cache stores compiled modules with MD5 checksums for validation.

## JavaScript/WebAssembly Performance

### Emscripten Target
```bash
make examplejs           # Compile and run Example.hs in Node.js
bin/mhs -temscripten Module -oout.js
node out.js
```

Performance is typically near-native when using modern JavaScript engines.

## Optimization Tips

### Compilation Performance
1. Use compilation cache (`-C`) for incremental builds
2. Avoid cache when modifying the compiler itself
3. Use `-z` flag for compressed combinator output
4. Module search path affects compilation speed

### Runtime Performance
1. Adjust heap size based on workload (larger heap = fewer GCs)
2. Use `-T` profiling to identify hot functions
3. Consider using GMP for Integer operations (uncomment in Makefile, rebuild)
4. Stack size may need adjustment for deeply recursive programs

### Build Configuration
- Default uses portable C implementation
- GMP support available for faster Integer operations
- Platform-specific configurations in `src/runtime/PLATFORM/`

## Comparative Performance

Typical performance characteristics:
- **Compilation speed**: Competitive with bytecode interpreters
- **Runtime speed**: ~60-70x slower than GHC -O2 for compute-intensive code
- **Memory usage**: Efficient due to combinator representation
- **Startup time**: Fast due to minimal runtime system

## Adding New Benchmarks

1. Create benchmark in `tests/` directory:
   - `MyBench.hs` - Source code
   - `MyBench.ref` - Expected output

2. Add to `tests/Makefile`:
   ```makefile
   $(TMHS) MyBench && $(EVAL) > MyBench.out && diff MyBench.ref MyBench.out
   ```

3. For timing benchmarks, use `System.IO.TimeMilli`:
   ```haskell
   import System.IO.TimeMilli
   
   main = do
     t1 <- getTimeMilli
     -- benchmark code
     t2 <- getTimeMilli
     putStrLn $ "Time: " ++ show (t2 - t1) ++ "ms"
   ```

## Performance Monitoring During Development

The `everytest` and `everytestmhs` targets run comprehensive test suites including performance tests, useful for ensuring changes don't regress performance.