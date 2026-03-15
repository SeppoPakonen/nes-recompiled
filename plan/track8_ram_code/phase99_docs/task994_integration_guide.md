# Task 994: Write Integration Guide

## Objective

Document how to integrate Track 8 with existing tracks and games.

## Content

### Integration with Existing Tracks

#### Track 1 (CPU)
- How RAM dispatch integrates with CPU dispatch
- Modified dispatch function
- No changes needed to instruction decoding

#### Track 4 (Mappers)
- MMC1 bank tracking for copy detection
- How mapper state affects ROM→RAM copies

#### Track 7 (Integration)
- Build system changes
- Executable size impact
- Runtime dependencies

### Step-by-Step Integration

1. **Add RAM cache to NESContext**
   ```c
   typedef struct NESContext {
       // Existing fields...
       RAMCodeCache ram_cache;  // Add this
   } NESContext;
   ```

2. **Initialize cache at startup**
   ```c
   ram_cache_init(&ctx->ram_cache);
   init_ram_cache(ctx);  // Pre-populated entries
   ```

3. **Modify dispatch**
   ```c
   void dispatch(NESContext* ctx, uint16_t addr) {
       if (addr < 0x0800) {
           RAMCodeEntry* entry = ram_cache_lookup(&ctx->ram_cache, addr, hash);
           if (entry) {
               entry->compiled_func(ctx);
               return;
           }
       }
       // Normal dispatch...
   }
   ```

4. **Hook write detection**
   ```c
   void nes_write8(NESContext* ctx, uint16_t addr, uint8_t value) {
       if (addr < 0x0800) {
           ctx->ram_dirty[addr >> 8] |= (1 << (addr & 0xFF));
       }
       // Normal write...
   }
   ```

### Migration from Interpreter

For games currently using interpreter fallback:
1. Enable trace collection
2. Run game to collect traces
3. Recompile with traces
4. Verify cache hit rate

### Testing

```bash
# Enable RAM cache stats
./game --ram-cache-stats

# Expected output:
# RAM Cache: hits=1234 misses=5 compilations=5 hit_rate=99.6%
```

## Acceptance Criteria

- [ ] integration_guide.md created
- [ ] Step-by-step instructions clear
- [ ] Code examples compile
- [ ] Migration path documented
- [ ] Testing instructions included
