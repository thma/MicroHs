#!/bin/bash
# MicroHs Profiling Analysis Wrapper Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE_FILE="${1:-mhs-profile.txt}"

echo "MicroHs Profiling Analysis"
echo "=========================="
echo

# Check if profile file exists
if [[ ! -f "$PROFILE_FILE" ]]; then
    echo "Error: Profile file '$PROFILE_FILE' not found."
    echo
    echo "To generate profiling data, run:"
    echo "  make timecompile-profile      # Profile compiler self-compilation"
    echo "  make runtestmhs-profile      # Profile test suite execution"
    echo "  make nfibtest-profile        # Profile Fibonacci benchmark"
    echo
    echo "Or manually with: bin/mhs +RTS -profile -RTS [args...]"
    exit 1
fi

echo "Analyzing profile data from: $PROFILE_FILE"
echo

# Run the Python analysis tool
python3 "$SCRIPT_DIR/analyze-profile.py" "$PROFILE_FILE" "$@"