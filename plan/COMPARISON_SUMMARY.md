# Three-Way Comparison Summary: Parasol Stars (a.nes)

**Date:** 2026-03-17
**Status:** Recompiled shows sprites (grey screen), Interpreter still broken

---

## Quick Comparison

| Feature | Reference | Recompiled (v13) | Interpreter (v14) |
|---------|-----------|------------------|-------------------|
| **Screen Output** | Red UI elements visible | Grey screen with 2 sprite images | Grey screen |
| **PPUCTRL** | Unknown | 0x90 ✓ | 0x00 ✗ |
| **PPUMASK** | Unknown | 0x10 (sprites) ✓ | 0x00 ✗ |
| **MMC1 Banking** | Working | Working ✓ | Stuck in loop ✗ |
| **CHR Banks** | Loaded | $00,$01,$02 ✓ | None ✗ |
| **Sprites** | 738 red pixels | 2 images visible | None |
| **Background** | Black | Not enabled | Not enabled |

---

## Key Findings

### ✅ What's Working (Recompiled v13)
- PPU registers configured correctly (PPUCTRL=0x90, PPUMASK=0x10)
- MMC1 banking working (PRG $00-$05, CHR $00-$02)
- Sprite rendering enabled
- **USER REPORTED:** "Shows grey with 2 images (sprites)" - PROGRESS!

### ❌ What's Broken (Recompiled v13)
- Background not enabled (PPUMASK bit 3 = 0)
- Palette may not be fully initialized
- Only 2 sprite images visible vs reference's full UI

### ❌ What's Broken (Interpreter v14)
- CPU stuck in MMC1 initialization loop (PC=0xC024-0xC027)
- PPU never configured (PPUCTRL=0x00, PPUMASK=0x00)
- No sprites rendered

---

## Root Causes

### Recompiled: Missing Background/Palette
The recompiled version has sprites working (user sees "2 images") but:
1. Background not enabled - PPUMASK should be 0x18 (BG+sprites) not 0x10
2. Palette may not be fully written
3. CHR data may be at wrong VRAM offset

### Interpreter: MMC1 Loop Bug
The interpreter is stuck in the MMC1 write sequence:
```assembly
INC $C024      ; Increment write counter
JSR $C035      ; Write to MMC1
BNE loop       ; Loop 256 times
```

The MMC1 handler isn't properly processing the 5-write serial sequence.

---

## Next Fixes (Priority Order)

### 1. Enable Background Rendering (Recompiled)
**File:** Generated code or game initialization

The game writes PPUMASK=0x10 (sprites only). This may be:
- Game bug (unlikely for commercial game)
- Recompiler misinterpreting the write value
- Game enables BG later in initialization

**Check:** Search for `STA $2001` in generated code and verify value

### 2. Fix Interpreter MMC1 Loop
**File:** `runtime/src/mapper.c` or recompiler MMC1 analysis

The MMC1 write handler needs to:
1. Track 5 consecutive writes to same address range
2. Shift bits into register
3. Commit on 5th write

**Debug:** Add logging to show write count and shift register state

### 3. Verify Palette Initialization
**Add logging:**
```c
[PPU] Write $2006=$3F00 (palette addr)
[PPU] Write $2007=$0F (palette[0] = black)
...
```

Compare palette values with reference emulator.

---

## Test Commands

```bash
# Test recompiled (should show sprites)
./output/a_v13/build/a roms/a.nes 2>&1 | grep -E "PPU.*write|MMC1"

# Test interpreter (still broken)
timeout 5 ./output/a_v14/build/a roms/a.nes --interpreter 2>&1 | grep "6502"

# Compare frame hashes
python3 scripts/analyze_frames.py
```

---

## Progress Summary

| Version | Status | Next Step |
|---------|--------|-----------|
| Reference | ✅ Working | N/A |
| Recompiled | 🟡 Partial (sprites visible) | Enable BG, fix palette |
| Interpreter | ❌ Broken (MMC1 loop) | Fix MMC1 handler |

**Overall Progress:** ~50% - Sprites working in recompiled, but full game not yet playable.

---

**References:**
- Full report: `plan/three_way_comparison_2026-03-17.md`
- NMI fix: `plan/track2_ppu/phase2_fixes/nmi_trigger_three_way.md`
- Speed fix: `plan/track5_runtime/phase2_fixes/interpreter_speed_three_way.md`

---

## Updated: v15 Status (Latest)

**Tested:** `./output/a_v15/build/a roms/a.nes`

### PPU Configuration (v15)
```
[PPU] PPUCTRL write: 0x90  ← NMI enabled, sprite table $1000
[PPU] PPUMASK write: 0x10  ← Sprites enabled, BG disabled
[PPU] Render scanline 0: CTRL=0x90 MASK=0x10 bg=0 spr=1
```

### MMC1 Banking (v15)
```
[MMC1] Control reg: 0x1E → 0x1D (vertical mirroring, 32KB PRG, 4KB CHR)
[MMC1] PRG bank: $01, $00, $03, $05
[MMC1] CHR bank 0: $00
[MMC1] CHR bank 1: $01, $02
```

### User Report
> "The ./output/a_v15/build/a shows finally something else than gray. 
> It's gray with 2 images (sprites)."

**Conclusion:** v15 is the latest working version with sprite rendering!

---

## Interpreter Status

The interpreter (`a_interp_test`) shows MMC1 RESET loop issues:
```
[MMC1] RESET
[MMC1] Write $9020 = $F7
[MMC1] RESET
...
```

This is different from the previous PC=0xC024-0xC027 loop.
The MMC1 handler may need debugging for the serial write sequence.
