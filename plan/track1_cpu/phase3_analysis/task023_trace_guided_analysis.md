# Task 023: Implement Trace-Guided Analysis

## Objective
Add trace-guided analysis to improve code coverage for complex ROMs.

## Problem
Static analysis may miss code paths in games with:
- Computed jumps (jump tables)
- Dynamic bank switching
- Self-modifying code
- RAM-resident code

## Solution
Use runtime execution traces to seed the analyzer with proven entry points.

## Changes Required

### 1. Add trace file parsing to analyzer
```cpp
static void load_trace_entry_points(const std::string& path, std::set<uint32_t>& call_targets) {
    // Read trace file with format:
    // bank:address (e.g., "0:C000")
    // Add each address to call_targets
}
```

### 2. Add --use-trace option to main.cpp
```cpp
if (arg == "--use-trace" && i + 1 < argc) {
    trace_file_path = argv[++i];
}
```

### 3. Update generated main.c to support trace output
```c
if (strcmp(argv[i], "--trace-entries") == 0 && i + 1 < argc) {
    nesrt_set_trace_file(argv[++i]);
}
```

### 4. Add nesrt_set_trace_file() to runtime
```c
void nesrt_set_trace_file(const char* filename) {
    // Open file for writing trace entries
}

void nesrt_log_trace(NESContext* ctx, uint16_t addr) {
    // Log address to trace file
}
```

## Usage Workflow

```bash
# Step 1: Run with trace output
./output/a_v2/build/a --trace-entries trace.log --limit 100000

# Step 2: Recompile using trace
./build/bin/nesrecomp roms/a.nes -o output/a_v3 --use-trace trace.log

# Step 3: Verify improved coverage
./build/bin/nesrecomp roms/a.nes -o output/a_v3 --verbose 2>&1 | grep functions
```

## Acceptance Criteria
- [ ] --use-trace option works
- [ ] --trace-entries option works in generated code
- [ ] Trace file format is correct (bank:address)
- [ ] Recompilation with trace finds more code
- [ ] Documented in README.md

## Dependencies
- Task 020 (analyzer working)
- Debug port logging (done)

## Estimated Effort
8-12 hours
