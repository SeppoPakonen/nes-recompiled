# Task 033: Generate RAM Dispatch Code

## Objective

Generate C code that dispatches RAM addresses to their pre-compiled ROM equivalents.

## Implementation

### Location
`recompiler/src/codegen/c_emitter.cpp`

### Generated Code Structure

```c
// In main dispatch function
static void dispatch(NESContext* ctx, uint16_t addr) {
    // Check RAM mappings first
    if (addr >= 0x0600 && addr < 0x0700) {
        // Map to ROM and call pre-compiled code
        uint16_t rom_addr = addr - 0x0600 + 0x8000;
        func_07_8000(ctx);  // Pre-compiled ROM function
        return;
    }
    
    // Normal ROM dispatch
    switch (addr) {
        case 0x8000: func_07_8000(ctx); break;
        // ...
    }
}

// Alternative: Direct function alias
#define func_ram_0600 func_07_8000
#define func_ram_0601 func_07_8001
// ...
```

### Codegen Changes

1. **Emit RAM mappings** in generated header
2. **Generate dispatch logic** for RAM regions
3. **Create function aliases** for RAM→ROM mapping

## Acceptance Criteria

- [ ] Generated code compiles without errors
- [ ] RAM dispatch is O(1) - direct function call
- [ ] No interpreter fallback for mapped RAM
- [ ] Comments document the mapping

## Testing

```bash
./build/bin/nesrecomp roms/a.nes -o output/test
grep -A5 "0x0600" output/test/a.c
# Should show RAM dispatch to ROM function
```
