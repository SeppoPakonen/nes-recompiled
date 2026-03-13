#!/bin/bash
# build_features.sh - Build all feature test ROMs
# Usage: ./build_features.sh [test_name]
# Examples:
#   ./build_features.sh         - Build all feature test ROMs
#   ./build_features.sh cpu     - Build test_cpu.nes only
#   ./build_features.sh ppu     - Build test_ppu.nes only

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== NES Feature Test ROM Builder ==="
echo "Directory: $SCRIPT_DIR"
echo ""

build_rom() {
    local test_name=$1
    local builder="build_test_${test_name}.py"
    local output="test_${test_name}.nes"

    if [[ -f "$builder" ]]; then
        echo "Building ${output}..."
        python3 "$builder"
        echo ""
    else
        echo "ERROR: Builder script '$builder' not found!"
        return 1
    fi
}

# List of all feature tests
FEATURE_TESTS="cpu ppu apu input timer"

# Build specific ROM or all ROMs
if [[ $# -gt 0 ]]; then
    build_rom "$1"
else
    # Build all feature test ROMs
    echo "Building all feature test ROMs..."
    echo ""
    for test_name in $FEATURE_TESTS; do
        build_rom "$test_name"
    done
fi

echo "=== Build Complete ==="
echo ""
echo "Generated ROMs:"
ls -lh test_*.nes 2>/dev/null || echo "No ROMs found"
echo ""
echo "Feature coverage summary:"
echo "  - test_cpu.nes:    6502 CPU instructions and flags"
echo "  - test_ppu.nes:    PPU registers, VRAM, sprites, palettes"
echo "  - test_apu.nes:    APU channels (pulse, triangle, noise, DMC)"
echo "  - test_input.nes:  Controller input reading"
echo "  - test_timer.nes:  Timing via cycles, frames, VBlank, NMI"
