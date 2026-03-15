# Task 032: Create RAM→ROM Mapping Data Structure

## Objective

Create data structures to store and query RAM→ROM mappings for pre-compiled code.

## Implementation

### Header: `recompiler/include/recompiler/ram_mapping.h`

```cpp
#pragma once
#include <cstdint>
#include <map>
#include <vector>

struct RAMCodeMapping {
    uint16_t ram_addr;      // RAM destination (0x0000-0x07FF)
    uint16_t rom_addr;      // ROM source address
    uint8_t rom_bank;       // ROM bank
    uint16_t length;        // Bytes copied
    bool verified;          // Destination is executed
};

class RAMMappingTable {
public:
    void add_mapping(const RAMCodeMapping& mapping);
    
    // Check if address has pre-compiled code
    bool has_mapping(uint16_t ram_addr) const;
    
    // Get ROM source for RAM address
    const RAMCodeMapping* get_mapping(uint16_t ram_addr) const;
    
    // Get all mappings
    const std::vector<RAMCodeMapping>& mappings() const;
    
private:
    std::map<uint16_t, RAMCodeMapping> by_ram_addr_;
    std::vector<RAMCodeMapping> all_mappings_;
};
```

### Integration

- Analyzer populates table during pattern detection
- Codegen queries table when generating dispatch code
- Runtime uses table for RAM code dispatch

## Acceptance Criteria

- [ ] Header file created with clean API
- [ ] O(log n) lookup by RAM address
- [ ] Serializes to generated C code
- [ ] Unit tests for basic operations

## Testing

```cpp
RAMMappingTable table;
table.add_mapping({0x0600, 0x8000, 7, 128, true});
assert(table.has_mapping(0x0600));
assert(!table.has_mapping(0x0700));
auto* m = table.get_mapping(0x0600);
assert(m->rom_addr == 0x8000);
assert(m->rom_bank == 7);
```
