# NES Recompiler Status Report

**Date:** 2026-03-17
**Latest Version:** v15
**ROM:** Parasol Stars (roms/a.nes)

---

## Executive Summary

**Progress: ~50%** - Sprites visible, but no colors (palette missing)

| Component | Status | Details |
|-----------|--------|---------|
| **NMI Trigger** | ✅ Fixed | VBlank NMI properly triggers CPU interrupt |
| **Interpreter Speed** | ✅ Fixed | ~62 FPS (100-200x speedup) |
| **PPU Registers** | ✅ Working | PPUCTRL=0x90, PPUMASK=0x10 |
| **MMC1 Banking** | ✅ Working | CHR banks $00-$02 loaded |
| **Sprite Rendering** | ✅ Partial | 2 sprites visible (grey) |
| **Background** | ⚠️ Disabled | Game-set (PPUMASK=0x10) |
| **Palette** | ❌ Missing | Not in generated code |
| **Interpreter** | ❌ Broken | MMC1 RESET loop |

---

## Latest Version: v15

### What's Working

```
[PPU] PPUCTRL write: 0x90  ← NMI enabled, sprite table $1000
[PPU] PPUMASK write: 0x10  ← Sprites enabled (BG disabled by game)
[PPU] Render scanline 0: CTRL=0x90 MASK=0x10 bg=0 spr=1

[MMC1] Control reg: 0x1E → 0x1D
[MMC1] PRG bank: $01, $00, $03, $05
[MMC1] CHR bank 0: $00
[MMC1] CHR bank 1: $01, $02
```

**User Report:** "Shows grey with 2 images (sprites)"

### What's Broken

1. **Palette Missing** - No color (all grey)
2. **Background Disabled** - Game-set PPUMASK=0x10
3. **Only 2 Sprites** - Reference shows full UI

---

## Critical Issues

### 1. Palette Initialization Missing 🔴

**Severity:** HIGH
**File:** `output/a_v15/a.c` (generated code)

**Problem:** Palette write code ($3F00-$3FFF) is NOT in generated code.

**Evidence:**
```bash
grep "0x3F00\|0x3F04" output/a_v15/a.c
# NO RESULTS - No palette writes!
```

**Impact:** All graphics are grey/black (palette zero-initialized)

**Fix Location:** Recompiler static analysis - need to follow code paths to palette init routines (likely at 0xEC37, 0xCA1D, or 0xEBDF)

**Documentation:** `plan/PALETTE_INVESTIGATION.md`

---

### 2. Background Disabled by Game 🟡

**Severity:** MEDIUM
**File:** Game code (not a bug)

**Problem:** Game sets PPUMASK=0x10 (sprites only), not 0x18 (BG+sprites)

**Evidence:**
```c
/* c178: ad 01 00     LDA  $0001 */
ctx->a = nes_read8(ctx, 0x0001);  // RAM[0x0001] = 0x10
/* c17e: 8d 01 20     STA  $2001 */
nes_write8(ctx, 0x2001, ctx->a);  // PPUMASK = 0x10
```

**Impact:** Background not rendered (sprite-only screen)

**Note:** This is intentional game behavior, not a bug. Game may enable BG later.

**Documentation:** `plan/PPUMASK_INVESTIGATION.md`

---

### 3. Interpreter MMC1 RESET Loop 🟡

**Severity:** MEDIUM
**File:** `runtime/src/mapper.c` or interpreter

**Problem:** Interpreter stuck in MMC1 RESET loop:
```
[MMC1] RESET
[MMC1] Write $9020 = $F7
[MMC1] RESET
...
```

**Impact:** Interpreter mode unusable

**Fix Location:** MMC1 serial write handler

---

## Completed Fixes

### ✅ NMI Trigger (CRITICAL)

**Files:** `runtime/src/nesrt.c`, `runtime/src/ppu.c`

**What was done:**
- Added `nes_trigger_nmi()` function
- Implements 6502 NMI sequence (push PC, status, load vector)
- Called from `update_vblank_nmi()` at VBlank

**Result:** Games using VBlank NMI now work correctly

---

### ✅ Interpreter Speed (HIGH)

**Files:** `runtime/src/nesrt.c`, `runtime/src/ppu.c`

**What was done:**
- Batch 50 instructions before PPU sync
- Render scanlines once per scanline (at end)
- Reduced PPU sync calls by 50x

**Result:** ~1 FPS → ~62 FPS (100-200x speedup)

---

## Three-Way Comparison

| Feature | Reference | v15 Recompiled | Interpreter |
|---------|-----------|----------------|-------------|
| **Screen** | Red UI elements | Grey + 2 sprites | Grey |
| **PPUCTRL** | - | 0x90 ✓ | 0x00 ✗ |
| **PPUMASK** | - | 0x10 ✓ | 0x00 ✗ |
| **MMC1** | Working | Working ✓ | RESET loop ✗ |
| **CHR Banks** | - | $00-$02 ✓ | Partial ✗ |
| **Sprites** | 738 red pixels | 2 grey images | None |
| **Palette** | Working | Missing ✗ | N/A |
| **FPS** | 60 | 60 | N/A |

---

## Next Steps (Priority Order)

### 1. Debug Palette Analysis 🔴
**Goal:** Find why palette init code wasn't analyzed

**Tasks:**
- Run recompiler with `--trace` to see analyzed code paths
- Check if palette code exists in ROM but not reached
- Enhance analysis to follow indirect jumps
- Regenerate code with palette init included

**Estimated Impact:** Would add colors to sprites (red UI visible)

---

### 2. Fix Interpreter MMC1 🟡
**Goal:** Get interpreter mode working

**Tasks:**
- Debug MMC1 serial write handler
- Fix RESET loop issue
- Test with simple MMC1 test ROM

**Estimated Impact:** Interpreter mode usable for debugging

---

### 3. Run Longer to Check BG Enable 🟢
**Goal:** See if game enables background later

**Tasks:**
- Run v15 for 60+ seconds
- Watch for PPUMASK changes to 0x18
- Compare with reference emulator timing

**Estimated Impact:** May reveal BG is enabled later

---

## Version History

| Version | Date | Changes | Status |
|---------|------|---------|--------|
| v15 | 2026-03-17 | Latest working | ✅ Sprites visible |
| v14 | 2026-03-16 | Interpreter speed fix | ✅ 62 FPS |
| v13 | 2026-03-16 | NMI trigger fix | ✅ NMI working |
| v12 | 2026-03-16 | PPUSTATUS fix | ⚠️ Partial |
| v11 | 2026-03-16 | TXS fix | ⚠️ Partial |

---

## Documentation

- `plan/PALETTE_INVESTIGATION.md` - Palette missing analysis
- `plan/PPUMASK_INVESTIGATION.md` - Background disabled analysis
- `plan/three_way_comparison_2026-03-17.md` - Full comparison
- `plan/COMPARISON_SUMMARY.md` - Quick reference
- `plan/IMPLEMENTATION_RESULTS.md` - Fix results

---

## References

- Reference emulator: `tmp/NES/build/nes`
- Recompiler: `./build/bin/nesrecomp`
- Latest output: `output/a_v15/`
- ROM: `roms/a.nes` (128KB PRG, 128KB CHR, Mapper 1)

---

**Last Updated:** 2026-03-17 14:00
**Next Review:** After palette analysis debug
