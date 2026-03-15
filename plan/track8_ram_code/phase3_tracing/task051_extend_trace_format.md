# Task 051: Extend Trace File Format for RAM

## Objective

Extend the trace file format to support RAM addresses.

## Implementation

### Format Change

Current:
```
7:C000
7:C035
```

Extended:
```
7:C000      # ROM: bank 7, address $C000
7:C035      # ROM: bank 7, address $C035
RAM:0600    # RAM: address $0600
RAM:0650    # RAM: address $0650
```

### Parser Changes

```cpp
void load_trace_entry_points(const std::string& path,
                             std::set<uint32_t>& call_targets,
                             std::set<uint16_t>& ram_targets) {
    std::ifstream file(path);
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.substr(0, 4) == "RAM:") {
            // RAM address
            uint16_t addr = std::stoi(line.substr(4), nullptr, 16);
            ram_targets.insert(addr);
        } else {
            // ROM address (existing format)
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                int bank = std::stoi(line.substr(0, colon));
                int addr = std::stoi(line.substr(colon + 1), nullptr, 16);
                call_targets.insert(make_address(bank, addr));
            }
        }
    }
}
```

### Runtime Changes

```c
// In nesrt_log_trace
void nesrt_log_trace(NESContext* ctx, uint16_t bank, uint16_t addr) {
    if (!ctx->trace_entries_enabled || !ctx->trace_file) return;
    
    if (addr < 0x0800) {
        fprintf(ctx->trace_file, "RAM:%04x\n", addr);
    } else {
        fprintf(ctx->trace_file, "%d:%04x\n", (int)bank, (int)addr);
    }
}
```

## Acceptance Criteria

- [ ] Trace files can contain both ROM and RAM entries
- [ ] Parser handles both formats correctly
- [ ] Runtime logs RAM addresses correctly
- [ ] Backward compatible with old trace files
