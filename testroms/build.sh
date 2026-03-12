#!/bin/bash
# build.sh - Build all test ROMs in the testroms directory
# Usage: ./build.sh [rom_number]
# Examples:
#   ./build.sh          - Build all test ROMs
#   ./build.sh 01       - Build test01_simple.nes
#   ./build.sh 02       - Build test02_transfers.nes
#   ./build.sh 03       - Build test03_addressing.nes

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== NES Test ROM Builder ==="
echo "Directory: $SCRIPT_DIR"
echo ""

build_rom() {
    local rom_num=$1
    local builder="build_test${rom_num}.py"
    local output="test${rom_num}_*.nes"

    if [[ -f "$builder" ]]; then
        echo "Building ${output}..."
        python3 "$builder"
        echo ""
    else
        echo "ERROR: Builder script '$builder' not found!"
        return 1
    fi
}

# Build specific ROM or all ROMs
if [[ $# -gt 0 ]]; then
    build_rom "$1"
else
    # Build all test ROMs (01, 02, 03, etc.)
    for num in 01 02 03 04 05; do
        builder="build_test${num}.py"
        if [[ -f "$builder" ]]; then
            build_rom "$num"
        fi
    done
fi

echo "=== Build Complete ==="
echo ""
echo "Generated ROMs:"
ls -lh *.nes 2>/dev/null || echo "No ROMs found"
