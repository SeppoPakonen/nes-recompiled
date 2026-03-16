# PPU Implementation Comparison

## Overview

This document compares the PPU implementation across three codebases:

1. **Reference Emulator** (`tmp/NES/src/ppu.c`) - Working NES emulator
2. **Our Runtime PPU** (`runtime/src/ppu.c`) - PPU for recompiled games
3. **Recompiled Code** (`output/a_v14/a.c`) - Generated C code from ROM

## Comparison Table

| Feature | Reference Emulator | Our Runtime PPU | Recompiled Code Access | Notes |
|---------|-------------------|-----------------|----------------------|-------|
| **VBlank Detection** | | | | |
| VBlank flag set | `ppu.c:292` - `ppu->status |= V_BLANK` at scanline 241 | `ppu.c:616` - `ppu->status |= PPUSTATUS_VBLANK` at scanline 241 | `a.c:9230-9233` - `BIT $2002` reads status | Runtime uses fast-forward workaround (lines 747-753) |
| VBlank flag cleared | `ppu.c:192` - Reading status clears `BIT_7` | `ppu.c:808` - Reading status clears `PPUSTATUS_VBLANK` | `a.c:9233` - `nes_read8` calls `ppu_read_register` | Both clear on read, matching hardware |
| VBlank NMI trigger | `ppu.c:204-209` - `update_NMI()` calls `interrupt(&cpu, NMI)` | `ppu.c:577-589` - `update_vblank_nmi()` sets `nmi_requested` | N/A - NMI handled by runtime | **BUG**: Runtime doesn't trigger CPU NMI |
| NMI clear | `ppu.c:300` - Cleared at pre-render scanline 261 | `ppu.c:623-625` - Cleared at scanline 261 | N/A | Runtime correctly clears NMI pending |
| **PPUSTATUS Register ($2002)** | | | | |
| Register read function | `ppu.c:187-193` - `read_status()` | `ppu.c:789-831` - `ppu_read_register()` case `0x02` | `a.c:9233` - `val = nes_read8(ctx, 0x2002)` | |
| Bit 7 (VBlank) behavior | `ppu.c:191` - `ppu->status &= ~BIT_7` after read | `ppu.c:808` - `ppu->status &= ~PPUSTATUS_VBLANK` | `a.c:9234` - `nes6502_bit` sets `f_n` | Correct: BPL/BNE after BIT work |
| Flag setting for CPU | N/A - emulator handles flags internally | `ppu.c:797-800` - **WORKAROUND**: Sets `ctx->f_n` and `ctx->f_z` | `a.c:9237,9254` - Uses `ctx->f_n` for BPL/BNE | **Critical fix**: Runtime sets flags so recompiled branches work |
| **Scanline Rendering** | | | | |
| Scanline counter | `ppu.c:213-317` - `ppu->scanlines` (0-261) | `ppu.c:598-634` - `ppu->scanline` (0-261) | N/A | |
| Visible scanlines | `ppu.c:214` - Render when `scanlines < 240` | `ppu.c:630-632` - Render when `scanline < 240` | N/A | |
| VBlank scanlines | `ppu.c:288-293` - Scanlines 241-261 | `ppu.c:614-619` - VBlank at scanline 241 | N/A | |
| Pre-render scanline | `ppu.c:296-311` - Scanline 261 | `ppu.c:620-626` - Scanline 261 | N/A | |
| Horizontal scroll | `ppu.c:263-270` - At dot 257, increments `v` | `ppu.c:266-268` - Uses `temp_vaddr` and `fine_x` | N/A | Reference updates `v` register |
| Vertical scroll | `ppu.c:272-286` - At dot 257, increments coarse Y | `ppu.c:266` - Calculated from `scanline + fine_y` | N/A | **DIFFERENCE**: Runtime calculates Y from scanline |
| **PPU Tick/Step Function** | | | | |
| Main step function | `ppu.c:213-317` - `execute_ppu()` | `ppu.c:598-634` - `ppu_step()` | N/A | |
| Cycle advancement | `ppu.c:313-317` - `dots++`, wrap at 341 | `ppu.c:599-611` - `cycle++`, wrap at 341 | N/A | |
| PPU to CPU ratio | N/A - runs separately | `ppu.c:740` - `ppu_cycles = cycles * 3` | N/A | Correct: 5.37 MHz vs 1.79 MHz |
| Fast-forward workaround | N/A | `ppu.c:743-753` - Fast-forwards to VBlank | `a.c:9230-9254` - Polling loops | **WORKAROUND**: Needed for interpreter mode |

---

## Critical Bugs Identified

### 1. NMI Generation Not Connected to CPU (CRITICAL)

**Reference Emulator (`tmp/NES/src/ppu.c:204-209`):**
```c
static void update_NMI(PPU* ppu) {
    if(ppu->ctrl & BIT_7 && ppu->status & BIT_7)
        interrupt(&ppu->emulator->cpu, NMI);  // Directly triggers CPU NMI
    else
        interrupt_clear(&ppu->emulator->cpu, NMI);
}
```

**Our Runtime (`runtime/src/ppu.c:577-589`):**
```c
static void update_vblank_nmi(NESPPU* ppu, NESContext* ctx) {
    if ((ppu->status & PPUSTATUS_VBLANK) && (ppu->ctrl & PPUCTRL_VBLANK_INT)) {
        if (!ppu->nmi_requested) {
            ppu->nmi_requested = 1;
            ppu->flags |= PPU_FLAG_NMI_PENDING;
            
            /* Trigger NMI in CPU */
            if (ctx) {
                ctx->ime = 1;  /* Enable interrupts */
                /* Set NMI vector - CPU will read from 0xFFFA */
            }
        }
    }
}
```

**Problem:** The runtime only sets `ctx->ime = 1` but never actually triggers the NMI interrupt. The reference emulator calls `interrupt(&cpu, NMI)` which sets the CPU's interrupt pending flag.

**Impact:** Games that rely on VBlank NMI will not work correctly. They will poll PPUSTATUS forever or miss the VBlank timing window.

**Fix Location:**
- **Reference:** `tmp/NES/src/ppu.c:204-209` - calls `interrupt()`
- **Runtime:** `runtime/src/ppu.c:577-589` - needs to call NMI trigger
- **Recompiled:** N/A - NMI handling is in runtime

---

### 2. Scroll Register Implementation Difference

**Reference Emulator:** Maintains `v`, `t`, `x`, `w` registers that track the PPU's internal scroll state.

**Our Runtime:** Calculates scroll position from `scanline` and `fine_x` during rendering.

**Reference (`tmp/NES/src/ppu.c:213-286`):**
```c
if(ppu->dots == VISIBLE_DOTS + 1 && ppu->mask & SHOW_BG){
    if((ppu->v & FINE_Y) != FINE_Y) {
        ppu->v += 0x1000;  // increment fine Y
    }
    // ... coarse Y increment with nametable switching
}
```

**Runtime (`runtime/src/ppu.c:264-268`):**
```c
int total_y = ppu->scanline + ((vaddr & PPU_VADDR_FINE_Y) >> 12);
uint8_t fine_y = total_y & 7;
uint8_t coarse_y = (total_y >> 3) & 0x1E;
```

**Impact:** Games that use raster effects (mid-scanline scroll changes) may not render correctly.

**Fix Location:**
- **Reference:** `tmp/NES/src/ppu.c:263-286` - updates `v` register
- **Runtime:** `runtime/src/ppu.c:264-268` - calculates from scanline
- **Recompiled:** N/A - scroll set via $2005 writes

---

### 3. Fast-Forward Workaround May Break Timing

**Runtime (`runtime/src/ppu.c:743-753`):**
```c
if (!fast_forward_done) {
    /* Fast-forward directly to VBlank */
    ppu->scanline = PPU_VBLANK_SCANLINE;
    ppu->cycle = 0;
    ppu->status |= PPUSTATUS_VBLANK;
    vblank_hold_ticks = 10;
    fast_forward_done = true;
}
```

**Problem:** This assumes games poll PPUSTATUS immediately on startup. Games with other initialization first may see incorrect behavior.

**Fix Location:**
- **Reference:** N/A - no fast-forward needed
- **Runtime:** `runtime/src/ppu.c:743-753` - workaround for interpreter mode
- **Recompiled:** `output/a_v14/a.c:9230-9254` - polling loops expect VBlank

---

### 4. Sprite Overflow Flag Timing

**Reference Emulator (`tmp/NES/src/ppu.c:269-277`):**
```c
if(ppu->OAM_cache_len >= 8)
    break;  // Stop evaluating, set overflow later
```

**Runtime (`runtime/src/ppu.c:317-320`):**
```c
} else {
    /* Sprite overflow - more than 8 sprites on scanline */
    ppu->sprite_overflow = 1;
    ppu->status |= PPUSTATUS_SPRITE_OVERFLOW;  // Set immediately
    break;
}
```

**Problem:** Hardware sets sprite overflow during sprite evaluation on scanline 261 (pre-render), not during visible scanlines.

**Fix Location:**
- **Reference:** `tmp/NES/src/ppu.c:269-277` - evaluates during pre-render
- **Runtime:** `runtime/src/ppu.c:317-320` - sets immediately
- **Recompiled:** N/A - sprite evaluation in runtime

---

## Summary of Critical Issues

| Priority | Issue | Impact | Fix Location |
|----------|-------|--------|--------------|
| **CRITICAL** | NMI not triggered in CPU | Games relying on VBlank NMI will not work | `runtime/src/ppu.c:577-589` |
| **HIGH** | Scroll calculation differs from hardware | Raster effects may break | `runtime/src/ppu.c:264-268` |
| **MEDIUM** | Fast-forward workaround | May break games with complex init | `runtime/src/ppu.c:743-753` |
| **LOW** | Sprite overflow timing | Minor sprite rendering issues | `runtime/src/ppu.c:317-320` |
| **LOW** | Palette mirroring | Minor color issues in edge cases | `runtime/src/ppu.c:94-103` |

---

## Recommended Fixes (Priority Order)

1. **Implement proper NMI triggering** - Add function to trigger 6502 NMI interrupt when VBlank NMI is enabled
2. **Implement proper scroll register emulation** - Track `v`, `t`, `x`, `w` registers like reference
3. **Remove or improve fast-forward** - Run PPU normally until first VBlank
4. **Fix sprite overflow timing** - Move evaluation to pre-render scanline
