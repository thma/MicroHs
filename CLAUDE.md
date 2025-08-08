# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MicroHs is a lightweight Haskell compiler implementation that uses combinators for runtime execution. The compiler can compile itself and has minimal dependencies, making it suitable even for microcontrollers.

## Build Commands

### Building the Compiler
- `make` - Build all main binaries (mhs, cpphs, mcabal)
- `make bin/mhs` - Build MicroHs compiler from C distribution
- `make bin/gmhs` - Build MicroHs using GHC
- `make newmhs` - Rebuild mhs with latest changes
- `make newmhsz` - Rebuild mhs with compression
- `make bootstrap` - Bootstrap the compiler (recompiles twice and compares outputs)

### Building Supporting Tools
- `make bin/cpphs` - Build C preprocessor for Haskell
- `make bin/mcabal` - Build MicroCabal package manager
- `make bin/mhseval` - Build combinator evaluator

### Testing Commands
- `make runtest` - Run tests using GHC-compiled compiler
- `make runtestmhs` - Run tests using MicroHs-compiled compiler
- `make runtestemscripten` - Run tests targeting JavaScript
- `make everytest` - Run all test suites

### Installation
- `make minstall` - Install mhs to ~/.mcabal
- `make oldinstall` - Install to /usr/local (or PREFIX)

## Compilation Usage

### Basic Usage
- `bin/mhs Module` - Compile module with main function
- `bin/mhs Module -oOutput` - Specify output file
- `bin/mhs -r Module` - Compile and run directly
- `bin/mhs` - Enter interactive REPL mode

### Important Flags
- `-i` - Clear module search path
- `-iDIR` - Add directory to module search path
- `-v` - Verbose output (can be repeated)
- `-C` - Use compilation cache (.mhscache)
- `-XCPP` - Enable C preprocessor
- `-tTARGET` - Select compilation target
- `-PPKG` - Create a package
- `-Q FILE [DIR]` - Install a package

## Architecture

### Core Components

1. **Compiler** (`src/MicroHs/`)
   - `Main.hs` - Entry point and flag processing
   - `Compile.hs` - Top-level compilation logic with module caching
   - `Parse.hs` - Parser and AST construction
   - `TypeCheck.hs` - Type checking implementation
   - `Desugar.hs` - Desugaring to simple expressions
   - `Abstract.hs` - Combinator abstraction and optimization
   - `Translate.hs` - Expression to combinator translation

2. **Runtime System** (`src/runtime/`)
   - `eval.c` - Core combinator evaluator with garbage collector
   - `main.c` - Runtime entry point
   - Platform-specific configurations in subdirectories (unix/, windows/, etc.)
   - `mir/` - MIR (Medium Internal Representation) JIT compiler infrastructure

3. **Standard Library** (`lib/`)
   - Complete Prelude implementation
   - Data structures (Array, ByteString, Text, etc.)
   - Control structures (Monad, Applicative, etc.)
   - Foreign Function Interface support
   - System interfaces (IO, Process, Directory, etc.)

### Key Design Decisions

- **Combinator-based evaluation**: Uses combinators rather than bytecode for execution
- **Scott encoding**: Types with < 5 constructors use Scott encoding, others use tagged tuples
- **Minimal dependencies**: Runtime system written in portable C
- **Self-hosting**: Compiler can compile itself
- **Package system**: Simple package format with all packages visible at compile time

## Development Workflow

### When modifying the compiler:
1. Make your changes to files in `src/MicroHs/`
2. Run `make newmhs` to build with your changes
3. Test with `make runtest` or `make runtestmhs`
4. For major changes, run `make bootstrap` to verify self-compilation

### When adding primitives:
1. Add enum variant in `src/runtime/eval.c`
2. Add entry to `primops` table in `eval.c`
3. Add case in `evali` function in `eval.c`
4. Add to `primTable` in `src/MicroHs/Translate.hs`
5. Add to `primOps` in `ghc/PrimTable.hs` (for GHC builds)

### Testing:
- Add new test: Create `MyTest.hs` and `MyTest.ref` in `tests/`
- Add to test rule in `tests/Makefile`
- For error tests, add to `tests/errmsg.test`

## Important Notes

- The compiler always enables many GHC extensions by default
- CPP is the only extension that needs explicit enabling with `-XCPP`
- Text I/O always uses UTF-8 encoding
- The runtime uses a mark-scan garbage collector
- Memory is based on cells (typically 16 bytes each)
- When using GMP for Integer, uncomment lines in Makefile and run `make clean`

## Environment Variables

- `MHSDIR` - Directory containing lib/ and src/ (defaults to ./)
- `MHSCC` - C compiler command
- `MHSCPPHS` - CPP command (defaults to cpphs)
- `MHSCONF` - Runtime configuration (e.g., unix-32, unix-64)
- `MHSEXTRACCFLAGS` - Extra C compiler flags

## Current Branch Status

Currently on branch: ASM
Main branch for PRs: master