# Task 052: Process RAM Traces in Analyzer

## Objective

Extend the analyzer to process RAM trace entries and create pre-compiled mappings.

## Implementation

### Analyzer Changes

```cpp
AnalysisResult analyze(const ROM& rom, const AnalyzerOptions& options) {
    AnalysisResult result;
    
    // Load trace entries (including RAM)
    std::set<uint16_t> ram_targets;
    load_trace_entry_points(options.trace_file_path, 
                           result.call_targets, 
                           ram_targets);
    
    // For each RAM target, find corresponding ROM source
    for (uint16_t ram_addr : ram_targets) {
        // Try to find ROM source via copy pattern detection
        RAMCodeMapping mapping = find_rom_source(rom, ram_addr);
        if (mapping.verified) {
            result.ram_mappings.add_mapping(mapping);
        }
    }
    
    // ... rest of analysis
}
```

### ROM Source Detection

```cpp
RAMCodeMapping find_rom_source(const ROM& rom, uint16_t ram_addr) {
    // Search for copy patterns that target this RAM address
    for (const auto& pattern : detected_patterns) {
        if (pattern.dst_start <= ram_addr && 
            ram_addr < pattern.dst_start + pattern.length) {
            return {
                .ram_addr = ram_addr,
                .rom_addr = pattern.src_start + (ram_addr - pattern.dst_start),
                .rom_bank = pattern.src_bank,
                .length = 1,  // Single address
                .verified = true
            };
        }
    }
    
    // No pattern found - mark for dynamic compilation
    return {
        .ram_addr = ram_addr,
        .rom_addr = 0,
        .rom_bank = 0,
        .length = 1,
        .verified = false  // Needs dynamic compilation
    };
}
```

## Acceptance Criteria

- [ ] Analyzer processes RAM trace entries
- [ ] Creates mappings for detected RAM code
- [ ] Marks undetected code for dynamic compilation
- [ ] Logs RAM analysis in verbose mode
