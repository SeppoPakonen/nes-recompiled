# MMC1 Bank Fix - Status Report

**Date:** 2026-03-17
**Status:** PARTIAL FIX - Root cause identified and partially fixed

---

## Root Cause Identified

The recompiler was missing palette initialization code because:

1. **Multiple banks at same address:** MMC1 ROMs have different code at the same CPU address depending on which PRG bank is mapped
2. **Function naming:** Bank 0 functions don't have bank prefix (`func_b5ec`), while other banks do (`func_01_b5ec`)
3. **Dispatch table bug:** When multiple functions exist at the same address, the dispatch table was using the alphabetically first function name, which was Bank 1+ instead of Bank 0
4. **Inlining bug:** Bank 0 functions were being marked as "inlineable" and skipped during code generation

---

## Fixes Applied

### 1. JSR/JMP Multi-Bank Analysis (analyzer.cpp)

Modified JSR and JMP handling to add targets in ALL banks for MMC1 switchable region ($8000-$BFFF):

```cpp
// For MMC1, add JSR targets in ALL banks for switchable region ($8000-$BFFF)
if (rom.header().mapper_number == 1 && target >= 0x8000 && target < 0xC000) {
    for (uint16_t b = 0; b < rom.header().rom_banks && b < 256; b++) {
        uint8_t tbank = static_cast<uint8_t>(b);
        result.call_targets.insert(make_address(tbank, target));
        work_queue.push({make_address(tbank, target), tbank});
    }
}
```

**Result:** All banks at JSR targets are now analyzed.

---

### 2. Dispatch Table Bank Preference (c_emitter.cpp)

Modified dispatch table generation to prefer Bank 0 functions when multiple banks have code at the same address:

```cpp
// For MMC1, prefer bank 0 function when multiple banks have code at same address
std::string selected_func = funcs[0].name;
for (const auto& entry : funcs) {
    // Check if this is a bank 0 function (name format: func_XXXX without bank prefix)
    if (entry.name.find("func_") == 0 && entry.name.size() == 9) {
        selected_func = entry.name;
        break;
    }
}
```

**Result:** Dispatch table now calls Bank 0 functions for addresses with multiple banks.

---

### 3. Prevent Bank 0 Inlining (c_emitter.cpp)

Modified inlinable function detection to NOT inline Bank 0 functions at addresses with multiple banks:

```cpp
// Don't inline Bank 0 functions at addresses that have multiple banks (MMC1)
// These need to be selectable from the dispatch table
if (func.bank == 0) {
    int banks_at_addr = 0;
    for (const auto& [other_name, other_func] : program.functions) {
        if (other_func.entry_address == func.entry_address) banks_at_addr++;
    }
    if (banks_at_addr > 1) continue;  // Skip inlining - needed for dispatch
}
```

**Result:** Bank 0 functions are now generated instead of being inlined.

---

## Test Results

### Test ROM (test_mmc1_palette.nes)

✅ **PASS** - Palette initialization code is now generated:

```c
loc_b5ec:
    /* b5ec: a9 3f        LDA  #$3f */
    ctx->a = 0x3f;
    /* b5ee: 8d 06 20     STA  $2006 */
    nes_write8(ctx, 0x2006, ctx->a);  // PPUADDR = $3F00 (palette)
    /* b5f1: a9 00        LDA  #$00 */
    ctx->a = 0x00;
    /* b5f3: 8d 06 20     STA  $2006 */
    nes_write8(ctx, 0x2006, ctx->a);
    /* b5f6: a9 0f        LDA  #$0f */
    ctx->a = 0x0f;  // Black color
    /* b5f8: 8d 07 20     STA  $2007 */
    nes_write8(ctx, 0x2007, ctx->a);  // Write to palette[0]
```

### Parasol Stars (a.nes)

⚠️ **PARTIAL** - PPU initialization improved but game still stuck:

**Progress:**
- ✅ PPUCTRL=0x90 written (NMI enabled, sprite table $1000)
- ✅ PPUMASK=0x10 written (sprites enabled)
- ✅ MMC1 banking working (CHR banks loaded)

**Issues:**
- ❌ Game gets stuck at PC=0x8E8A (unanalyzed address)
- ❌ Step counter overflow (display bug, not functional)
- ❌ No palette writes detected yet

**Root Cause:** The fix prefers Bank 0 for ALL addresses with multiple banks, but some addresses should use different banks depending on MMC1 state. Proper fix requires MMC1 state tracking during analysis.

---

## Remaining Issues

### 1. MMC1 State Tracking (HIGH Priority)

The current fix assumes Bank 0 should always be used for addresses with multiple banks. This is incorrect - the correct bank depends on the MMC1 PRG bank register state at runtime.

**Proper Fix:** Track MMC1 register state during analysis and generate code that switches banks at runtime.

**Complexity:** HIGH - Requires significant recompiler architecture changes.

---

### 2. Function Naming Ambiguity (MEDIUM Priority)

Bank 0 functions are named `func_XXXX` while other banks are `func_NN_XXXX`. This causes:
- Alphabetical sorting issues in dispatch table
- Confusion about which bank a function is from

**Fix:** Always include bank prefix: `func_00_XXXX` for Bank 0.

---

### 3. Analysis Performance (LOW Priority)

Analyzing all banks at all JSR targets increases analysis time and memory usage.

**Impact:** Acceptable for now (test ROM analyzes in < 1 second).

---

## Files Modified

1. `recompiler/src/analyzer.cpp` - JSR/JMP multi-bank analysis
2. `recompiler/src/codegen/c_emitter.cpp` - Dispatch table and inlining fixes

---

## Next Steps

1. **Short-term:** Fix function naming to always include bank prefix
2. **Medium-term:** Add MMC1 state tracking for correct bank selection
3. **Long-term:** Full mapper support with runtime bank switching

---

## Conclusion

The root cause of the missing palette code has been identified and partially fixed. The test ROM now generates correct palette initialization code. However, Parasol Stars still has issues due to incorrect bank selection at some addresses.

The fundamental issue is that static analysis cannot determine which MMC1 bank is active at each JSR target without tracking MMC1 register state. A proper fix requires either:
1. Runtime bank switching in generated code
2. More sophisticated static analysis with MMC1 state tracking

Both approaches require significant recompiler changes.
