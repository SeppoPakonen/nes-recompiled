# Task 042: Implement RAM Write Detection and Hashing

## Objective

Detect when code is written to RAM and compute hashes for cache lookup.

## Implementation

### Write Detection

Hook into `nes_write8` / `nes_write16` to detect RAM writes:

```c
// In runtime/src/nesrt.c
void nes_write8(NESContext* ctx, uint16_t addr, uint8_t value) {
    if (addr < 0x0800) {  // RAM write
        ctx->ram_dirty[addr >> 8] |= (1 << (addr & 0xFF));
        ctx->ram_hash_valid[addr >> 8] = 0;  // Invalidate hash
    }
    // ... normal write logic
}
```

### Hash Computation

```c
uint64_t compute_ram_hash(NESContext* ctx, uint16_t start, uint16_t length) {
    if (ctx->ram_hash_valid[start >> 8]) {
        return ctx->ram_hash[start >> 8];
    }
    
    uint64_t hash = fnv1a_hash(ctx->ram + start, length);
    ctx->ram_hash[start >> 8] = hash;
    ctx->ram_hash_valid[start >> 8] = 1;
    return hash;
}
```

### Dirty Tracking

```c
typedef struct NESContext {
    uint8_t ram[0x0800];           // 2KB RAM
    uint8_t ram_dirty[8];          // Dirty bitmap (256 bits per byte)
    uint8_t ram_hash_valid[8];     // Hash validity per page
    uint64_t ram_hash[8];          // Cached hashes per page
    // ...
} NESContext;
```

## Acceptance Criteria

- [ ] Detects all RAM writes
- [ ] Hash computation is fast (<1μs for 256 bytes)
- [ ] Dirty tracking doesn't slow down normal execution
- [ ] Hash invalidation works correctly
