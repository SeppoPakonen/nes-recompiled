# Phase 3: Trace Integration for RAM Code

## Objective

Extend trace-guided analysis to capture RAM execution traces and use them for pre-compilation.

## Background

Static analysis can't discover all RAM code, but runtime traces can. By capturing RAM addresses during execution, we can:
1. Pre-compile RAM code before it's executed
2. Populate the cache ahead of time
3. Avoid runtime compilation delays

## Approach

### 1. Extended Trace Format

Current format: `bank:address`
Extended format: `bank:address` or `RAM:address`

```
# Trace file example
7:C000      # ROM bank 7, address $C000
7:C035      # ROM bank 7, address $C035
RAM:0600    # RAM address $0600
RAM:0650    # RAM address $0650
```

### 2. Trace Collection

Modify `nesrt_log_trace` to handle RAM addresses:

```c
void nesrt_log_trace(NESContext* ctx, uint16_t bank, uint16_t addr) {
    if (!ctx->trace_entries_enabled || !ctx->trace_file) return;
    
    if (addr < 0x0800) {
        // RAM address
        fprintf(ctx->trace_file, "RAM:%04x\n", addr);
    } else {
        // ROM address
        fprintf(ctx->trace_file, "%d:%04x\n", (int)bank, (int)addr);
    }
}
```

### 3. Trace Analysis

Extend analyzer to process RAM traces:

```cpp
void load_trace_entry_points(const std::string& path,
                             std::set<uint32_t>& call_targets,
                             std::set<uint16_t>& ram_targets) {
    // Parse trace file
    // ROM entries -> call_targets
    // RAM entries -> ram_targets
}
```

### 4. Pre-populate Cache

Generate executable with pre-populated RAM cache:

```c
// Generated in a.c
RAMCodeEntry precompiled_ram[] = {
    {0x12345678, func_07_8000_mapped, 0x0600, 128, 1, NULL},
    {0x87654321, func_07_8100_mapped, 0x0700, 64, 1, NULL},
    // ...
};

void init_ram_cache(NESContext* ctx) {
    for (auto& entry : precompiled_ram) {
        ram_cache_insert(&ctx->ram_cache, entry.ram_addr, 
                        entry.hash, entry.compiled_func);
    }
}
```

## Tasks

- [ ] **task051_extend_trace_format.md**: Extend trace file format for RAM
- [ ] **task052_analyze_ram_traces.md**: Process RAM traces in analyzer
- [ ] **task053_prepopulate_cache.md**: Generate pre-populated cache in executable

## Expected Results

After Phase 3:
- Trace files include RAM addresses
- Analyzer discovers RAM code from traces
- Generated executable has pre-populated RAM cache
- Zero runtime compilation for traced games

## Dependencies

- Phase 1: Static detection (provides initial mappings)
- Phase 2: Dynamic recompilation (provides cache infrastructure)
- Track 7: Trace-guided analysis infrastructure
