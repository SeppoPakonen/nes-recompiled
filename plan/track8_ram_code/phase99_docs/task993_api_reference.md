# Task 993: Create API Reference

## Objective

Document all public APIs for Track 8 RAM code handling.

## API Categories

### Runtime API (C)

```c
// Cache management
void ram_cache_init(RAMCodeCache* cache);
RAMCodeEntry* ram_cache_lookup(RAMCodeCache* cache, uint16_t addr, uint64_t hash);
void ram_cache_insert(RAMCodeCache* cache, uint16_t addr, uint64_t hash, void (*func)(NESContext*));

// Recompilation
void* ram_recompile(NESContext* ctx, uint16_t ram_addr, uint16_t length);
void ram_free_compiled(void* func);

// Hash computation
uint64_t ram_compute_hash(const uint8_t* ram, uint16_t start, uint16_t length);
```

### Analyzer API (C++)

```cpp
// Pattern detection
std::vector<CopyPattern> detect_copy_patterns(const ROM& rom);

// RAM mapping
class RAMMappingTable {
    void add_mapping(const RAMCodeMapping& mapping);
    bool has_mapping(uint16_t ram_addr) const;
    const RAMCodeMapping* get_mapping(uint16_t ram_addr) const;
};
```

### Generated Code API

```c
// Generated in executable
void init_ram_cache(NESContext* ctx);
extern RAMCodeEntry ram_cache_init[];
```

## Documentation Format

For each function:
- Name and signature
- Description
- Parameters
- Return value
- Example usage
- Thread safety
- Performance characteristics

## Acceptance Criteria

- [ ] API.md created
- [ ] All public functions documented
- [ ] Includes examples
- [ ] Parameter descriptions complete
- [ ] Return values documented
