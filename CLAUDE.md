# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

MicroHs is an implementation of an extended subset of Haskell that compiles to combinators for runtime execution. The runtime system has minimal dependencies and can compile even for micro-controllers. The compiler can compile itself and supports both GHC and self-compilation modes.

## Build Commands

### Primary Build Targets
- `make` - Build all primary binaries: `bin/mhs`, `bin/cpphs`, `bin/mcabal`
- `make bin/mhs` - Compile MicroHs with C compiler (from distribution combinator file)
- `make bin/gmhs` - Compile MicroHs with GHC
- `make bin/cpphs` - Build C preprocessor
- `make bin/mhseval` - Build combinator evaluator only
- `make newmhs` - Generate new compiler with latest changes using GHC
- `make newmhsz` - Same as newmhs but with compression
- `make bootstrap` - Full bootstrap process with stage1/stage2 comparison

### Testing
- `make runtest` - Run all tests with GHC-compiled compiler
- `make runtestmhs` - Run all tests with MicroHs-compiled compiler  
- `make runtestemscripten` - Run tests via JavaScript/WASM compilation
- `make runtestsan` - Run tests with sanitized compiler
- `cd tests && make alltest` - Run test suite directly

### Development
- `make clean` - Clean all build artifacts
- `make timecompile` - Time compilation performance
- `make cachelib` - Precompile library cache for faster builds
- `make exampletest` - Test with Example.hs

### Installation  
- `make install` - Install to ~/.mcabal (recommended)
- `make minstall` - Install binaries and base packages to ~/.mcabal
- `make oldinstall` - Install to /usr/local (requires MHSDIR env var)

## Architecture

### Compiler Pipeline
The compilation process flows through these main phases:
1. **Lexing & Parsing** (`MicroHs.Lex`, `MicroHs.Parse`) - Convert source to AST
2. **Type Checking** (`MicroHs.TypeCheck`) - Verify types and resolve overloading
3. **Desugaring** (`MicroHs.Desugar`) - Transform high-level constructs to simple expressions
4. **Translation** (`MicroHs.Translate`) - Convert to combinator expressions
5. **Abstract** (`MicroHs.Abstract`) - Perform bracket abstraction and optimization
6. **Code Generation** (`MicroHs.ExpPrint`, `MicroHs.MakeCArray`) - Output C code or combinators

### Core Modules
- `MicroHs.Main` - Entry point, argument parsing, orchestration
- `MicroHs.Compile` - Top-level compilation coordination and caching
- `MicroHs.Exp` - Simple expression representation (post-desugaring)
- `MicroHs.Expr` - Full expression AST (pre-desugaring)  
- `MicroHs.Ident` - Identifier management and namespace handling
- `MicroHs.SymTab` - Symbol table operations
- `MicroHs.Flags` - Compiler flag processing
- `MicroHs.Interactive` - REPL implementation

### Runtime System
- Written in C (`src/runtime/eval.c`)
- Uses combinators for variable handling
- Simple mark-scan garbage collector  
- Minimal dependencies, portable across platforms
- Configuration files in `src/runtime/*/config.h` for different targets

### Library Structure
- `lib/` - Core library modules (Prelude, base functionality)
- `ghc/`, `hugs/`, `mhs/` - Compiler-specific overrides and compatibility shims
- `boards/` - Microcontroller examples and configurations

### Target Configuration
- `targets.conf` - Defines compilation targets (default, emscripten, tcc, windows)
- Cross-compilation support via target selection (`-tTARGET`)
- Runtime configuration via `MHSCONF` environment variable

## Key Environment Variables
- `MHSDIR` - Installation directory (defaults to `./`)
- `MHSCC` - C compiler command override
- `MHSCPPHS` - C preprocessor command (defaults to `cpphs`)
- `MHSCONF` - Runtime configuration (defaults to `unix-32/64`)
- `MHSEXTRACCFLAGS` - Additional C compiler flags

## Interactive Development
- Run `bin/mhs` without arguments to enter REPL mode
- Commands: `:quit`, `:clear`, `:del STR`, `:reload`, `:type EXPR`, `:kind TYPE`
- History saved in `.mhsi`, definitions saved in `Interactive.hs`

## Package System
- Create packages: `mhs -Ppackage-name.pkg modules...`
- Install packages: `mhs -Q package-name.pkg [install-dir]`  
- Search path controlled by `-a` flags
- MicroCabal (`mcabal`) provides Hackage-compatible package management

## Bootstrapping Process
The compiler can bootstrap itself completely:
1. Start with distributed combinator file (`generated/mhs.c`)
2. Compile stage1 with existing compiler
3. Compile stage2 with stage1, verify identical to stage1
4. Can also bootstrap from Hugs for complete bootstrappability

## Testing Framework
- Test files in `tests/` with `.hs` source and `.ref` expected output
- Error message tests in `tests/errmsg.test`
- Add tests by creating `Test.hs` and `Test.ref`, then adding to `tests/Makefile`
- Foreign function interface tests via `testforimp` and `testforexp`

## Common Development Patterns
- Use compilation cache (`-C` flag) for faster development iterations
- Run `make newmhs` when modifying compiler to rebuild with changes
- Use verbose flags (`-v`) for debugging compilation performance
- Test with multiple targets (native, emscripten) for portability
- Always run tests before committing changes

## FFI and Cross-Platform Support
- Supports calling C functions with full FFI
- Platform-specific runtime configurations in `src/runtime/*/`
- Emscripten target for JavaScript/WASM compilation
- Microcontroller examples in `boards/` directory