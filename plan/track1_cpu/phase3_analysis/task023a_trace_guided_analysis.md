# Task 023a: Implement Trace-Guided Analysis

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
**File:** `recompiler/src/analyzer.cpp`

```cpp
static void load_trace_entry_points(const std::string& path, std::set<uint32_t>& call_targets) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open trace file: " << path << "\n";
        return;
    }

    std::string line;
    int count = 0;
    while (std::getline(file, line)) {
        // Parse format: "bank:address" or "address"
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            try {
                int bank = std::stoi(line.substr(0, colon));
                int addr = std::stoi(line.substr(colon + 1), nullptr, 16);
                call_targets.insert(make_address(bank, addr));
                count++;
            } catch (...) {
                continue;
            }
        } else {
            // Just address (assume bank 0)
            try {
                int addr = std::stoi(line, nullptr, 16);
                call_targets.insert(make_address(0, addr));
                count++;
            } catch (...) {
                continue;
            }
        }
    }
    std::cout << "Loaded " << count << " entry points from trace file: " << path << "\n";
}
```

### 2. Add --use-trace option to main.cpp
**File:** `recompiler/src/main.cpp`

```cpp
std::string trace_file_path;

// In argument parsing:
} else if (arg == "--use-trace" && i + 1 < argc) {
    trace_file_path = argv[++i];
}

// Pass to analyzer options:
analyze_opts.trace_file_path = trace_file_path;
```

### 3. Add trace output to generated main.c
**File:** `recompiler/src/codegen/c_emitter.cpp`

```cpp
// In generated main function:
main_ss << "    if (strcmp(argv[i], \"--trace-entries\") == 0 && i + 1 < argc) {\n";
main_ss << "        nesrt_set_trace_file(argv[++i]);\n";
main_ss << "    }\n";
```

### 4. Add runtime trace functions
**File:** `runtime/src/nesrt.c`

```c
static FILE* g_trace_file = NULL;

void nesrt_set_trace_file(const char* filename) {
    if (g_trace_file) fclose(g_trace_file);
    g_trace_file = fopen(filename, "w");
    if (g_trace_file) {
        fprintf(stderr, "[NESRT] Tracing entry points to %s\n", filename);
    }
}

void nesrt_log_trace(NESContext* ctx, uint16_t addr) {
    if (g_trace_file) {
        fprintf(g_trace_file, "0:%04X\n", addr);
        fflush(g_trace_file);
    }
}
```

**File:** `runtime/include/nesrt.h`

```c
void nesrt_set_trace_file(const char* filename);
void nesrt_log_trace(NESContext* ctx, uint16_t addr);
```

### 5. Call trace logging in dispatch
**File:** Generated code or runtime

```c
void nes_dispatch(NESContext* ctx, uint16_t addr) {
    nesrt_log_trace(ctx, addr);  // Log entry point
    // ... existing dispatch logic
}
```

## Usage Workflow

```bash
# Step 1: Run with trace output
./output/a_v3/build/a --trace-entries trace.log --limit 100000

# Step 2: Recompile using trace
./build/bin/nesrecomp roms/a.nes -o output/a_v4 --use-trace trace.log

# Step 3: Verify improved coverage
./build/bin/nesrecomp roms/a.nes -o output/a_v4 --verbose 2>&1 | grep functions
```

## Acceptance Criteria
- [ ] --use-trace option works
- [ ] --trace-entries option works in generated code
- [ ] Trace file format is correct (bank:address)
- [ ] Recompilation with trace finds more code
- [ ] Documented in README.md

## Dependencies
- Task 021b (basic testing working)

## Estimated Effort
8-12 hours
