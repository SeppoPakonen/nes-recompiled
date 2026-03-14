#!/bin/bash
#
# Trace-Guided Recompilation Script for NES Recompiler
# 
# This script automatically performs iterative trace-guided analysis:
# 1. Recompiles ROM with current trace file (if exists)
# 2. Builds the generated project
# 3. Runs with trace enabled to collect entry points
# 4. Repeats until no new entry points are found
#
# Usage: ./scripts/trace_guided_recomp.sh <rom_file> [output_dir] [max_iterations]
#

set -e

# Configuration
RECOMPILER="./build/bin/nesrecomp"
MAX_INSTRUCTIONS=1000000
MAX_ITERATIONS=${3:-10}
RUN_FRAMES=300  # Number of frames to run for trace collection

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse arguments
ROM_FILE="$1"
OUTPUT_BASE="${2:-output/trace_guided}"

if [ -z "$ROM_FILE" ]; then
    echo "Usage: $0 <rom_file> [output_dir] [max_iterations]"
    echo ""
    echo "Arguments:"
    echo "  rom_file       Path to the NES ROM file"
    echo "  output_dir     Base directory for output (default: output/trace_guided)"
    echo "  max_iterations Maximum iterations (default: 10)"
    exit 1
fi

if [ ! -f "$ROM_FILE" ]; then
    echo -e "${RED}Error: ROM file not found: $ROM_FILE${NC}"
    exit 1
fi

# Extract ROM name without extension
ROM_NAME=$(basename "$ROM_FILE" .nes)
TRACE_FILE="logs/${ROM_NAME}_trace.log"
MERGED_TRACE="logs/${ROM_NAME}_merged_trace.log"

# Ensure logs directory exists
mkdir -p logs

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Trace-Guided Recompilation${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "ROM: $ROM_FILE"
echo "Output base: $OUTPUT_BASE"
echo "Trace file: $TRACE_FILE"
echo "Max iterations: $MAX_ITERATIONS"
echo "Max instructions per run: $MAX_INSTRUCTIONS"
echo ""

# Function to count unique entries in trace file
count_entries() {
    if [ -f "$1" ]; then
        sort -u "$1" | wc -l
    else
        echo "0"
    fi
}

# Function to merge trace files
merge_traces() {
    local old_trace="$1"
    local new_trace="$2"
    local merged="$3"
    
    if [ -f "$old_trace" ] && [ -f "$new_trace" ]; then
        cat "$old_trace" "$new_trace" | sort -u > "$merged"
    elif [ -f "$new_trace" ]; then
        cp "$new_trace" "$merged"
    fi
}

# Initialize
iteration=0
prev_entries=0

if [ -f "$TRACE_FILE" ]; then
    prev_entries=$(count_entries "$TRACE_FILE")
    echo -e "${YELLOW}Found existing trace file with $prev_entries unique entries${NC}"
fi

# Main iteration loop
while [ $iteration -lt $MAX_ITERATIONS ]; do
    iteration=$((iteration + 1))
    OUTPUT_DIR="${OUTPUT_BASE}_v${iteration}"
    
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Iteration $iteration / $MAX_ITERATIONS${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    # Step 1: Recompile with trace file if it exists
    echo ""
    echo -e "${GREEN}Step 1: Recompiling ROM...${NC}"
    
    if [ -f "$MERGED_TRACE" ]; then
        echo "Using merged trace file: $MERGED_TRACE"
        $RECOMPILER "$ROM_FILE" -o "$OUTPUT_DIR" --use-trace "$MERGED_TRACE" --verbose 2>&1 | grep -E "functions|IR blocks|Loaded.*entry"
    elif [ -f "$TRACE_FILE" ]; then
        echo "Using trace file: $TRACE_FILE"
        $RECOMPILER "$ROM_FILE" -o "$OUTPUT_DIR" --use-trace "$TRACE_FILE" --verbose 2>&1 | grep -E "functions|IR blocks|Loaded.*entry"
    else
        echo "No trace file, doing initial analysis"
        $RECOMPILER "$ROM_FILE" -o "$OUTPUT_DIR" --verbose 2>&1 | grep -E "functions|IR blocks"
    fi
    
    # Step 2: Build the generated project
    echo ""
    echo -e "${GREEN}Step 2: Building generated project...${NC}"
    
    cmake -G Ninja -S "$OUTPUT_DIR" -B "${OUTPUT_DIR}/build" > /dev/null 2>&1
    ninja -C "${OUTPUT_DIR}/build" > /dev/null 2>&1
    
    if [ ! -f "${OUTPUT_DIR}/build/${ROM_NAME}" ]; then
        echo -e "${RED}Error: Build failed!${NC}"
        exit 1
    fi
    
    echo "Build successful: ${OUTPUT_DIR}/build/${ROM_NAME}"
    
    # Step 3: Run with trace enabled
    echo ""
    echo -e "${GREEN}Step 3: Running ROM to collect trace...${NC}"
    
    ITER_TRACE="logs/${ROM_NAME}_iter_${iteration}.log"
    
    # Run and capture output
    set +e
    timeout 30 "${OUTPUT_DIR}/build/${ROM_NAME}" \
        --trace-entries "$ITER_TRACE" \
        --limit "$MAX_INSTRUCTIONS" \
        2>&1 | tail -5
    set -e
    
    if [ ! -f "$ITER_TRACE" ]; then
        echo -e "${RED}Error: Trace file not created!${NC}"
        exit 1
    fi
    
    # Count entries
    iter_entries=$(count_entries "$ITER_TRACE")
    echo "Trace entries this iteration: $iter_entries"
    
    # Step 4: Merge traces
    echo ""
    echo -e "${GREEN}Step 4: Merging trace files...${NC}"
    
    merge_traces "$MERGED_TRACE" "$ITER_TRACE" "$MERGED_TRACE.new"
    mv "$MERGED_TRACE.new" "$MERGED_TRACE"
    
    merged_entries=$(count_entries "$MERGED_TRACE")
    echo "Total unique entries after merge: $merged_entries"
    
    # Also update the main trace file
    cp "$MERGED_TRACE" "$TRACE_FILE"
    
    # Step 5: Check for convergence
    new_entries=$((merged_entries - prev_entries))
    
    echo ""
    if [ $new_entries -gt 0 ]; then
        echo -e "${YELLOW}Found $new_entries new entry points${NC}"
        prev_entries=$merged_entries
    else
        echo -e "${GREEN}No new entry points found - analysis complete!${NC}"
        break
    fi
done

# Summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Analysis Complete${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Results:"
echo "  Iterations: $iteration"
echo "  Total unique entry points: $(count_entries "$MERGED_TRACE")"
echo "  Final output: ${OUTPUT_DIR}"
echo "  Trace file: $TRACE_FILE"
echo "  Merged trace: $MERGED_TRACE"
echo ""
echo "To run the final recompiled ROM:"
echo "  ${OUTPUT_DIR}/build/${ROM_NAME}"
echo ""
echo "To recompile with the collected trace:"
echo "  $RECOMPILER $ROM_FILE -o output/final --use-trace $MERGED_TRACE"
echo ""
