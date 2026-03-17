# MMC1 Bank Tracking Fix - Status Report

**Date:** 2026-03-17
**Status:** PARTIAL - Root cause identified, fix too expensive

---

## Root Cause Confirmed

**The palette initialization code at 0xB5EC exists in Bank 0, but the analyzer was only analyzing Bank 1 at that address.**

### Evidence
- File offset 13820 = Bank 0, address 0xB5EC
- Contains: `LDA #$3F; STA $2006` (palette init)
- Generated code has Bank 1 at 0xB5EC (just `SEC` instruction)
- Bank 0 at 0xB5EC was NEVER ANALYZED

---

## Fix Attempted

### Changes Made

1. **Analyzer (`analyzer.cpp`):**
   - Added MMC1 multi-bank analysis for switchable region ($8000-$BFFF)
   - Queues analysis for ALL banks at each address in switchable region

2. **Code Generator (`c_emitter.cpp`):**
   - Include functions from ALL banks (not just bank 0 and last)
   - Add bank switch calls before cross-bank JSR

3. **Runtime (`mapper.c/h`):**
   - Added `nes_mapper_set_prg_bank()` for runtime bank switching
   - Added `nes_mapper_get_prg_bank()` to query current bank

### Problem: Analysis Too Slow

**Memory usage:** 6.6GB+ and growing
**Time:** > 3 minutes without completing

**Reason:** Analyzing ALL banks at ALL addresses in $8000-$BFFF creates exponential blowup:
- 8 banks × 16384 addresses = 131,072 address/bank combinations
- Each combination needs full control flow analysis
- Memory and time requirements are prohibitive

---

## Alternative Approaches

### Option 1: Selective Multi-Bank Analysis (Recommended)

Only analyze multiple banks at addresses that are:
1. JSR/JMP targets (already done in prescan)
2. After known bank switch instructions

This would require:
- Identifying MMC1 write patterns ($8000-$BFFF writes with serial protocol)
- Tracking when bank switches occur
- Re-analyzing code paths after each bank switch

**Complexity:** HIGH
**Performance:** Should be acceptable

### Option 2: Two-Pass Analysis

**Pass 1:** Analyze with current bank context tracking
**Pass 2:** For each unanalyzed address, try all banks

This would catch missed code without analyzing everything in all banks.

**Complexity:** MEDIUM
**Performance:** Should be acceptable

### Option 3: Runtime Bank Dispatch

Generate code that checks bank at runtime:
```c
void func_at_b5ec() {
    switch (current_bank) {
        case 0: func_00_b5ec(); break;  // Palette init
        case 1: func_01_b5ec(); break;  // SEC instruction
        // ...
    }
}
```

**Complexity:** MEDIUM
**Performance:** Runtime overhead but acceptable

### Option 4: Manual Palette Init (Workaround)

Add palette initialization to runtime as a workaround:
```c
void ppu_load_palette(NESPPU* ppu) {
    // Hardcoded palette from ROM analysis
    ppu->palette[0x00] = 0x0F;  // Black
    ppu->palette[0x01] = 0x30;  // White
    ppu->palette[0x02] = 0x16;  // Red
    // ...
}
```

**Complexity:** LOW
**Performance:** N/A (one-time init)
**Drawback:** Not a real fix, just a workaround

---

## Current Status

### What Works
- ✅ NMI trigger
- ✅ Interpreter speed (62 FPS)
- ✅ Sprite rendering (grey only)
- ✅ MMC1 banking (for recompiled code)

### What's Broken
- ❌ Palette initialization (code in wrong bank)
- ❌ Background rendering (game-set, not a bug)
- ❌ Interpreter MMC1 loop

### Progress: ~50%

---

## Next Steps

### Immediate (Workaround)
1. Add manual palette initialization to runtime
2. Test if colors appear with hardcoded palette
3. This would prove the root cause and provide playable output

### Long-term (Proper Fix)
1. Implement selective multi-bank analysis (Option 1)
2. Or two-pass analysis (Option 2)
3. Test with multiple MMC1 games

---

## Files Modified

- `recompiler/src/analyzer.cpp` - Multi-bank analysis (reverted/too slow)
- `recompiler/src/codegen/c_emitter.cpp` - Bank switch calls
- `runtime/src/mapper.c` - Bank set/get functions
- `runtime/include/nesrt_mapper.h` - Bank set/get declarations

---

## Conclusion

**Root cause confirmed:** Palette code in Bank 0 at 0xB5EC, analyzer only looked at Bank 1.

**Fix blocked:** Brute-force multi-bank analysis is too expensive (6.6GB+ memory).

**Recommended:** Implement selective multi-bank analysis OR add manual palette workaround for now.
