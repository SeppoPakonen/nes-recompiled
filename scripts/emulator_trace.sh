#!/bin/bash
#
# Emulator Trace Extraction Script
# 
# This script runs a ROM using the recompiled interpreter mode to capture
# the actual execution trace, then uses that trace for guided recompilation.
#
# The interpreter logs every executed address, giving us accurate code paths
# that the game actually uses.
#
# Usage: ./scripts/emulator_trace.sh <rom_file> [output_dir] [instructions]
#

set -e

# Configuration
RECOMPILER="./build/bin/nesrecomp"
MAX_INSTRUCTIONS=${3:-1000000}

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Parse arguments
ROM_FILE="$1"
OUTPUT_BASE="${2:-output/emulator_trace}"

if [ -z "$ROM_FILE" ]; then
    echo "Usage: $0 <rom_file> [output_dir] [max_instructions]"
    echo ""
    echo "This script:"
    echo "  1. Recompiles ROM with interpreter fallback enabled"
    echo "  2. Runs the ROM to capture execution trace"
    echo "  3. Recompiles using the captured trace"
    echo ""
    echo "Arguments:"
    echo "  rom_file       Path to the NES ROM file"
    echo "  output_dir     Base directory for output (default: output/emulator_trace)"
    echo "  instructions   Max instructions to trace (default: 1000000)"
    exit 1
fi

if [ ! -f "$ROM_FILE" ]; then
    echo -e "${RED}Error: ROM file not found: $ROM_FILE${NC}"
    exit 1
fi

# Extract ROM name
ROM_NAME=$(basename "$ROM_FILE" .nes)
TRACE_FILE="logs/${ROM_NAME}_emu_trace.log"
MERGED_TRACE="logs/${ROM_NAME}_merged_trace.log"

# Ensure directories exist
mkdir -p logs
mkdir -p "$OUTPUT_BASE"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Emulator Trace Extraction${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "ROM: $ROM_FILE"
echo "Output: $OUTPUT_BASE"
echo "Trace file: $TRACE_FILE"
echo "Max instructions: $MAX_INSTRUCTIONS"
echo ""

# Step 1: Initial recompile (to get interpreter)
echo -e "${CYAN}Step 1: Initial recompilation...${NC}"
$RECOMPILER "$ROM_FILE" -o "${OUTPUT_BASE}_initial" --verbose 2>&1 | grep -E "functions|IR blocks"

# Step 2: Build
echo -e "${CYAN}Step 2: Building...${NC}"
cmake -G Ninja -S "${OUTPUT_BASE}_initial" -B "${OUTPUT_BASE}_initial/build" > /dev/null 2>&1
ninja -C "${OUTPUT_BASE}_initial/build" > /dev/null 2>&1

if [ ! -f "${OUTPUT_BASE}_initial/build/${ROM_NAME}" ]; then
    echo -e "${RED}Error: Build failed!${NC}"
    exit 1
fi

# Step 3: Run with trace
echo -e "${CYAN}Step 3: Running ROM to capture trace...${NC}"
echo "This will run the ROM and log all executed addresses..."
echo ""

timeout 60 "${OUTPUT_BASE}_initial/build/${ROM_NAME}" \
    --trace-entries "$TRACE_FILE" \
    --limit "$MAX_INSTRUCTIONS" \
    2>&1 | tail -10

if [ ! -f "$TRACE_FILE" ]; then
    echo -e "${RED}Error: Trace file not created!${NC}"
    exit 1
fi

# Analyze trace
TOTAL_LINES=$(wc -l < "$TRACE_FILE")
UNIQUE_ENTRIES=$(sort -u "$TRACE_FILE" | wc -l)

echo ""
echo -e "${GREEN}Trace captured successfully!${NC}"
echo "  Total entries: $TOTAL_LINES"
echo "  Unique entry points: $UNIQUE_ENTRIES"
echo ""

# Show top entry points
echo -e "${CYAN}Top 20 most executed addresses:${NC}"
sort "$TRACE_FILE" | uniq -c | sort -rn | head -20 | while read count addr; do
    printf "  %8x  %d times\n" "0x${addr#*:}" "$count"
done
echo ""

# Step 4: Recompile with trace
echo -e "${CYAN}Step 4: Recompiling with trace-guided analysis...${NC}"
$RECOMPILER "$ROM_FILE" -o "${OUTPUT_BASE}_final" --use-trace "$TRACE_FILE" --verbose 2>&1 | grep -E "functions|IR blocks|Loaded.*entry"

# Step 5: Build final version
echo -e "${CYAN}Step 5: Building final version...${NC}"
cmake -G Ninja -S "${OUTPUT_BASE}_final" -B "${OUTPUT_BASE}_final/build" > /dev/null 2>&1
ninja -C "${OUTPUT_BASE}_final/build" > /dev/null 2>&1

if [ ! -f "${OUTPUT_BASE}_final/build/${ROM_NAME}" ]; then
    echo -e "${RED}Error: Final build failed!${NC}"
    exit 1
fi

# Summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Trace Extraction Complete${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Results:"
echo "  Trace file: $TRACE_FILE"
echo "  Unique entry points: $UNIQUE_ENTRIES"
echo "  Final output: ${OUTPUT_BASE}_final"
echo ""
echo "To run the final recompiled ROM:"
echo "  ${OUTPUT_BASE}_final/build/${ROM_NAME}"
echo ""
echo "To merge with existing traces:"
echo "  cat $TRACE_FILE logs/*_merged_trace.log | sort -u > $MERGED_TRACE"
echo ""
