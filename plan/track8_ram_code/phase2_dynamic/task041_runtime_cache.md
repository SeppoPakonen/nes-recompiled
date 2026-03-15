# Task 041: Implement RAM Code Cache Data Structure

## Objective

Create the runtime cache data structure for storing compiled RAM code.

## Implementation

### Header: `runtime/include/nesrt_ram_cache.h`

```c
#pragma once
#include <stdint.h>

#define RAM_CACHE_BITS 8
#define RAM_CACHE_SIZE (1 << RAM_CACHE_BITS)

typedef struct RAMCodeEntry {
    uint64_t hash;                      // Hash of RAM contents
    void (*compiled_func)(struct NESContext*);  // Compiled function
    uint16_t ram_addr;                  // RAM address
    uint16_t length;                    // Code length
    uint8_t is_valid;                   // Entry is valid
    struct RAMCodeEntry* next;          // Collision chain
} RAMCodeEntry;

typedef struct RAMCodeCache {
    RAMCodeEntry entries[RAM_CACHE_SIZE];
    RAMCodeEntry* buckets[RAM_CACHE_SIZE];
    uint32_t hits;
    uint32_t misses;
    uint32_t compilations;
} RAMCodeCache;

// API
void ram_cache_init(RAMCodeCache* cache);
RAMCodeEntry* ram_cache_lookup(RAMCodeCache* cache, uint16_t ram_addr, uint64_t hash);
void ram_cache_insert(RAMCodeCache* cache, uint16_t ram_addr, uint64_t hash, 
                      void (*func)(struct NESContext*));
void ram_cache_stats(RAMCodeCache* cache);
```

### Implementation: `runtime/src/nesrt_ram_cache.c`

```c
// FNV-1a hash for bucket selection
static inline uint32_t hash_bucket(uint64_t hash) {
    return (hash ^ (hash >> 32)) & (RAM_CACHE_SIZE - 1);
}

RAMCodeEntry* ram_cache_lookup(RAMCodeCache* cache, uint16_t ram_addr, uint64_t hash) {
    uint32_t bucket = hash_bucket(hash);
    RAMCodeEntry* entry = cache->buckets[bucket];
    
    while (entry) {
        if (entry->ram_addr == ram_addr && entry->hash == hash && entry->is_valid) {
            cache->hits++;
            return entry;
        }
        entry = entry->next;
    }
    
    cache->misses++;
    return NULL;
}
```

## Acceptance Criteria

- [ ] O(1) average lookup time
- [ ] Handles hash collisions correctly
- [ ] Thread-safe (if needed for future)
- [ ] Statistics tracking for debugging

## Testing

```c
RAMCodeCache cache;
ram_cache_init(&cache);

// Insert
ram_cache_insert(&cache, 0x0600, 0x12345678, my_func);

// Lookup
RAMCodeEntry* e = ram_cache_lookup(&cache, 0x0600, 0x12345678);
assert(e != NULL);
assert(e->compiled_func == my_func);

// Stats
ram_cache_stats(&cache);  // Should show 1 hit
```
