# Palette Code Location Analysis

**Date:** 2026-03-17
**Finding:** Palette code exists in ROM but in WRONG BANK

---

## Root Cause: MMC1 Bank Mapping Issue

**The palette initialization code EXISTS in the ROM but is in bank 0, not bank 1!**

### Palette Code Location

**In ROM at file offset 13820:**
```assembly
0xB5EC (Bank 0):
    LDA #$3F
    STA $2006      ; PPUADDR = $3F00 (palette)
    LDA #$00
    STA $2006      ; PPUADDR = $3F00
    LDA ($0E),Y    ; Load color from indirect
    STA $2007      ; Write to palette
```

**File offset calculation:**
- File offset 13820 - 16 (iNES header) = PRG offset 13804 = 0x35EC
- Bank 0 (first 16KB)
- CPU address: 0x8000 + (0x35EC % 0x4000) = **0xB5EC in Bank 0**

### Generated Code Problem

The generated code has:
```c
/* 01:b5ec: SEC */  // Bank 1, NOT Bank 0!
```

**Bank 1 at 0xB5EC contains:** `0x38` (SEC instruction)
**Bank 0 at 0xB5EC contains:** Palette initialization code!

### Why This Happened

The MMC1 mapper switches PRG banks:
- $8000-$BFFF can be any 16KB PRG bank
- $C000-$FFFF is fixed to last bank

**The recompiler analyzed:**
- Bank 1 mapped to $8000-$BFFF (where palette code isn't)
- Bank 0 at 0xB5EC was NEVER ANALYZED

**What should happen:**
- Game switches to Bank 0 at $8000-$BFFF
- Jumps to 0xB5EC (which is now palette code)
- Palette is initialized

---

## Evidence

### ROM Content Comparison

**Bank 0 at 0xB5EC (file offset 13820):**
```
0000360c: 3f8d 0620 a900 8d06 20b1 0e38 e5af b002  ?.. .... ..8....
          ^^ ^^ ^^ ^^ ^^ ^^ ^^ ^^
          3F 8D 06 20 A9 00 8D 06  <- Palette init!
```

**Bank 1 at 0xB5EC (file offset 30204):**
```
000075f4: 9003 fef5 03ad 5c03 38e9 089d 4803 a50e  ......\.8...H...
                                        ^^
                                        38  <- Just SEC instruction
```

### Generated Code

```bash
grep "01:b5ec" output/a_v15/a.c
# Shows: /* 01:b5ec: SEC */

grep "00:b5ec" output/a_v15/a.c
# NO RESULTS - Bank 0 at 0xB5EC was never analyzed!
```

---

## Impact

**The palette code is never executed because:**
1. Recompiler analyzed Bank 1 at 0xB5EC (just SEC instruction)
2. Bank 0 at 0xB5EC (palette code) was never reached
3. Generated code calls the wrong function

---

## Fix Required

### Option 1: Enhance MMC1 Analysis (Correct Fix)

The recompiler needs to:
1. Track MMC1 register state during analysis
2. Analyze ALL possible bank configurations
3. Generate code for each bank at each address
4. Switch between bank functions based on MMC1 state

**This is a MAJOR recompiler architecture change.**

### Option 2: Force Analyze All Banks (Workaround)

Add command-line option to analyze all banks at all addresses:
```bash
./build/bin/nesrecomp roms/a.nes --analyze-all-banks -o output/a_v17
```

This would generate code for:
- func_00_b5ec (Bank 0 - palette code) ✓
- func_01_b5ec (Bank 1 - SEC instruction)
- func_02_b5ec (Bank 2 - ???)
- etc.

### Option 3: Manual Bank Switch Injection (Hack)

Add explicit bank switch before palette init call:
```c
// In generated code where palette init should be called
nes_mmc1_set_bank(0);  // Switch to bank 0
func_00_b5ec(ctx);     // Call palette code
```

**This requires knowing WHERE palette init should be called.**

---

## Next Steps

1. **Find where palette init is called from:**
   - Search for JSR to 0xB5EC in ROM
   - Check if call happens when Bank 0 is mapped

2. **Add MMC1 tracking to recompiler:**
   - Track PRG bank register during analysis
   - Generate code for all bank/address combinations

3. **Regenerate with all banks analyzed:**
   - Force analysis of Bank 0 at 0xB5EC
   - Generate func_00_b5ec with palette code

---

## Conclusion

**This is NOT a missing code issue - the palette code EXISTS.**

**This IS a bank mapping issue - the recompiler analyzed the wrong bank at 0xB5EC.**

**Fix requires:** Enhanced MMC1 bank tracking in recompiler analysis phase.

---

## Related Files

- ROM: `roms/a.nes` (file offset 13820 = Bank 0, 0xB5EC)
- Generated: `output/a_v15/a.c` (has Bank 1 at 0xB5EC, not Bank 0)
- MMC1 handler: `runtime/src/mapper.c`
