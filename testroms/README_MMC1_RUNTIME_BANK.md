# MMC1 Runtime Bank Switching Test

**Test ROM:** `testroms/test_mmc1_runtime_bank.nes`
**Build Script:** `testroms/build_test_mmc1_runtime_bank.py`

---

## Purpose

This test ROM demonstrates the fundamental limitation of static analysis for MMC1 (and other mappers with bank switching):

**The same CPU address can contain different code depending on the MMC1 PRG bank register state at runtime.**

Static analysis cannot determine which bank is active at each JSR without tracking MMC1 register writes throughout the entire codebase.

---

## ROM Structure

**Size:** 64KB (4 × 16KB PRG banks)
**Mapper:** MMC1 (mapper 1)

| Bank | File Offset | CPU Address (when mapped) | Code at 0xB5EC |
|------|-------------|---------------------------|----------------|
| **Bank 0** | 0x0010-0x400F | $8000-$BFFF | `LDA #$01; STA $0000; RTS` |
| **Bank 1** | 0x4010-0x800F | $8000-$BFFF | `LDA #$02; STA $0000; RTS` |
| **Bank 2** | 0x8010-0xC00F | $8000-$BFFF | `LDA #$03; STA $0000; RTS` |
| **Bank 3** | 0xC010-0x1000F | $C000-$FFFF (FIXED) | `LDA #$04; STA $0000; RTS` |

**MMC1 Behavior:**
- `$8000-$BFFF`: Switchable (banks 0-2)
- `$C000-$FFFF`: Fixed to last bank (bank 3)

---

## Test Code

The main code (in Bank 3, fixed at CPU $C000-$FFFF):

1. **Test 1:** Switch to Bank 0, JSR $B5EC, expect RAM $00 = $01
2. **Test 2:** Switch to Bank 1, JSR $B5EC, expect RAM $00 = $02
3. **Test 3:** Switch to Bank 2, JSR $B5EC, expect RAM $00 = $03
4. **Test 4:** (Bank 3 is fixed, can't switch) JSR $B5EC, expect RAM $00 = $04

Results stored in:
- RAM $0002: Result from Bank 0 call
- RAM $0003: Result from Bank 1 call
- RAM $0004: Result from Bank 2 call
- RAM $0005: Result from Bank 3 call

---

## Static Analysis Challenge

### The Problem

When the static analyzer sees `JSR $B5EC`, it cannot determine which bank's code will execute because:

1. **Bank depends on MMC1 state:** The PRG bank register determines which 16KB bank is mapped to $8000-$BFFF
2. **State is set at runtime:** MMC1 registers are written via a 5-bit serial protocol
3. **Multiple code paths:** Different code paths may set different banks before the JSR

### Example

```assembly
; Path 1: Bank 0 active
LDA #$80
STA $A000  ; Reset MMC1
LDA #$00   ; Bank 0
STA $A000  ; (5 times)
...
JSR $B5EC  ; Executes Bank 0 code (LDA #$01)

; Path 2: Bank 1 active
LDA #$80
STA $A000  ; Reset MMC1
LDA #$01   ; Bank 1
STA $A000  ; (5 times)
...
JSR $B5EC  ; Executes Bank 1 code (LDA #$02)
```

Both paths JSR to the same address ($B5EC), but execute different code!

---

## Current Recompiler Behavior

### What Works

✅ All 4 banks at 0xB5EC are analyzed:
- `func_b5ec` (Bank 0)
- `func_01_b5ec` (Bank 1)
- `func_02_b5ec` (Bank 2)
- `func_03_b5ec` (Bank 3)

✅ Dispatch table prefers Bank 0 for addresses with multiple banks

### What Doesn't Work

❌ **No runtime bank switching:** Generated code always calls Bank 0's function, regardless of MMC1 state

❌ **No MMC1 state tracking:** Analyzer doesn't track which bank is active at each JSR

❌ **Address mapping issue:** Main code at CPU $C000 is at offset $8000 in Bank 3, causing dispatch issues

---

## Expected vs Actual Behavior

| Test | Expected RAM Value | Actual (Current Fix) |
|------|-------------------|---------------------|
| Bank 0 call | $01 | $01 (correct - Bank 0 preferred) |
| Bank 1 call | $02 | $01 (wrong - should be Bank 1) |
| Bank 2 call | $03 | $01 (wrong - should be Bank 2) |
| Bank 3 call | $04 | $01 (wrong - should be Bank 3) |

---

## Proper Solution

To correctly handle MMC1 bank switching, the recompiler needs:

### Option 1: Runtime Bank Dispatch

Generate code that checks the current MMC1 bank at runtime:

```c
void jsr_b5ec() {
    switch (nes_mapper_get_prg_bank(&ctx->mapper)) {
        case 0: func_b5ec(ctx); break;
        case 1: func_01_b5ec(ctx); break;
        case 2: func_02_b5ec(ctx); break;
        case 3: func_03_b5ec(ctx); break;
    }
}
```

**Pros:** Correct behavior
**Cons:** Runtime overhead, requires mapper state tracking

### Option 2: Static Analysis with MMC1 Tracking

Track MMC1 register state during analysis:

1. Analyze code path from reset
2. Track MMC1 writes ($8000-$BFFF)
3. Determine active bank at each JSR
4. Generate code for correct bank

**Pros:** No runtime overhead
**Cons:** Complex analysis, may miss dynamic code paths

---

## Files

- `testroms/build_test_mmc1_runtime_bank.py` - Build script
- `testroms/test_mmc1_runtime_bank.nes` - Test ROM
- `output/test_mmc1_runtime_bank/` - Generated code

---

## Related Issues

- Parasol Stars palette code in Bank 0 at 0xB5EC
- MMC1 state tracking during analysis
- Runtime bank switching for generated code

---

## Build and Test

```bash
# Build ROM
cd testroms
python3 build_test_mmc1_runtime_bank.py

# Recompile
./build/bin/nesrecomp testroms/test_mmc1_runtime_bank.nes -o output/test_mmc1_runtime_bank

# Build and run
cmake -G Ninja -S output/test_mmc1_runtime_bank -B output/test_mmc1_runtime_bank/build
ninja -C output/test_mmc1_runtime_bank/build
./output/test_mmc1_runtime_bank/build/test_mmc1_runtime_bank roms/test_mmc1_runtime_bank.nes
```

---

**Date:** 2026-03-17
**Status:** Demonstrates fundamental limitation of static analysis for mappers
