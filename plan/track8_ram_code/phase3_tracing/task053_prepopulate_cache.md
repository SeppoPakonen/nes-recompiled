# Task 053: Generate Pre-populated RAM Cache

## Objective

Generate executable with pre-populated RAM cache from trace analysis.

## Implementation

### Generated Code

```c
// In generated a.c

// Pre-compiled RAM code entries
static RAMCodeEntry ram_cache_init[] = {
    // Hash, function, addr, length, valid, next
    {0x1234567890ABCDEFULL, func_07_8000, 0x0600, 128, 1, NULL},
    {0xFEDCBA0987654321ULL, func_07_8100, 0x0700, 64, 1, NULL},
    // ... more entries
    {0, NULL, 0, 0, 0, NULL}  // Sentinel
};

void init_ram_cache(NESContext* ctx) {
    ram_cache_init_table(&ctx->ram_cache);
    
    for (RAMCodeEntry* e = ram_cache_init; e->is_valid; e++) {
        ram_cache_insert(&ctx->ram_cache, e->ram_addr, 
                        e->hash, e->compiled_func);
    }
}
```

### Codegen Changes

```cpp
// In c_emitter.cpp
void emit_ram_cache_init(const AnalysisResult& result) {
    source_ss << "static RAMCodeEntry ram_cache_init[] = {\n";
    
    for (const auto& mapping : result.ram_mappings) {
        std::string func_name = make_function_name(
            mapping.rom_bank, mapping.rom_addr);
        uint64_t hash = compute_hash_for_mapping(mapping);
        
        source_ss << "    {0x" << std::hex << hash << std::dec
                  << ", " << func_name 
                  << ", 0x" << std::hex << mapping.ram_addr << std::dec
                  << ", " << mapping.length
                  << ", 1, NULL},\n";
    }
    
    source_ss << "    {0, NULL, 0, 0, 0, NULL}  // Sentinel\n";
    source_ss << "};\n\n";
}
```

## Acceptance Criteria

- [ ] Generated code compiles without errors
- [ ] Cache is pre-populated at startup
- [ ] No runtime compilation for pre-populated entries
- [ ] Cache statistics show high hit rate

## Testing

```bash
./build/bin/nesrecomp roms/a.nes -o output/test --use-trace logs/a_trace.log
grep "ram_cache_init" output/test/a.c
# Should show pre-populated entries
```
