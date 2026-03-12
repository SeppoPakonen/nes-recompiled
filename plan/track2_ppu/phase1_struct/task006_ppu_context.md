# Task 006: PPU Context Structure

## Objective
Create PPU context structure for runtime.

## Background
NES PPU has:
- 2KB VRAM (nametables + pattern tables)
- 256 bytes OAM (sprite attributes)
- Palette RAM (32 bytes)
- Memory-mapped registers ($2000-$2007)

## Changes Required

### 1. Create `runtime/include/nesrt_ppu.h`
```c
typedef struct {
    uint8_t ctrl;           // $2000
    uint8_t mask;           // $2001
    uint8_t status;         // $2002
    uint8_t oam_addr;       // $2003
    uint8_t oam_data;       // $2004
    uint8_t scroll;         // $2005
    uint16_t vram_addr;     // $2006
    uint8_t vram_data;      // $2007
    
    uint8_t vram[0x800];    // 2KB VRAM (mirrored)
    uint8_t oam[256];       // 64 sprites * 4 bytes
    uint8_t palette[32];    // 4 background + 4 sprite palettes * 4 colors
    
    // Rendering state
    uint16_t scanline;
    uint16_t cycle;
    uint8_t frame_complete;
    
    // Output
    uint32_t framebuffer[256 * 240];  // ARGB8888
} NESPPU;
```

### 2. Create `runtime/src/ppu.c`
- `nes_ppu_init()`
- `nes_ppu_read()` - handle $2002/$2007
- `nes_ppu_write()` - handle $2000-$2007
- `nes_ppu_step()` - scanline rendering
- `nes_ppu_render_scanline()`

## Acceptance Criteria
- [ ] PPU registers readable/writable
- [ ] VRAM/OAM accessible
- [ ] Basic scanline rendering

## Dependencies
None (parallel track)

## Estimated Effort
8-16 hours

## Reference
NES emulator PPU at `/home/sblo/Ohjelmat/NES/src/ppu.c`
