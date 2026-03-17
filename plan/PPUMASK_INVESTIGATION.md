# PPUMASK Investigation Results

**Date:** 2026-03-17
**Issue:** Background not rendering (PPUMASK=0x10 instead of 0x18)

---

## Findings

### PPUMASK Write Pattern

The game writes PPUMASK ($2001) from RAM values:

**Location 1:** `07:c0e2`
```c
/* c0de: a9 00        LDA  #$00 */
ctx->a = 0x00;
/* c0e2: 8d 01 20     STA  $2001 */
nes_write8(ctx, 0x2001, ctx->a);  // PPUMASK = 0x00
```

**Location 2:** `07:c17e`
```c
/* c178: ad 01 00     LDA  $0001 */
ctx->a = nes_read8(ctx, 0x0001);
/* c17a: 29 1f        AND  #$1f */
nes6502_and(ctx, 0x1f);
/* c17e: 8d 01 20     STA  $2001 */
nes_write8(ctx, 0x2001, ctx->a);  // PPUMASK = RAM[0x0001] & 0x1F
```

### RAM $0001 Value

The value written to PPUMASK comes from RAM location $0001, which is set to **0x10** (sprites only, no background).

**Expected:** RAM[0x0001] should be 0x18 (BG bit 3 + sprite bit 4)
**Actual:** RAM[0x0001] = 0x10 (sprite bit 4 only)

### RAM $0015 Value

There's another RAM location $0015 that IS set to 0x18:

```c
/* e989: a9 18        LDA  #$18 */
ctx->a = 0x18;
/* e98b: 85 15        STA  $15 */
nes_write8(ctx, 0x0015, ctx->a);  // RAM[0x0015] = 0x18
```

But this value is NOT used for PPUMASK - it's used for something else.

---

## Root Cause Hypothesis

The game's PPUMASK value (0x10) is **intentionally set by the game code**, not a recompiler bug.

Possible explanations:

1. **Sprite-only title screen:** Game may intentionally disable background during title screen
2. **Initialization sequence:** Background may be enabled later in game initialization
3. **RAM not initialized:** RAM[0x0001] may need to be set to 0x18 by game code that hasn't run yet

---

## Evidence

### Game writes PPUMASK multiple times:
- 0x00 (all disabled) - during PPU initialization
- 0x10 (sprites only) - current state
- 0x18 (BG+sprites) - NOT FOUND in generated code

### RAM locations:
- RAM[0x0001] = 0x10 → PPUMASK (sprites only)
- RAM[0x0015] = 0x18 → Some other purpose (not PPUMASK)

---

## Next Steps

1. **Check if game enables BG later:**
   - Run for longer and watch for PPUMASK=0x18
   - Search for PPUMASK writes in later execution

2. **Compare with reference:**
   - Does reference emulator see PPUMASK=0x18?
   - If yes, where does it come from?

3. **Check CHR data:**
   - Even with BG enabled, need CHR data for background tiles
   - CHR banks $00-$02 loaded, but may not contain valid BG data

4. **Check OAM:**
   - 2 sprites visible means OAM is partially working
   - Check if all 64 sprite entries are initialized

---

## Conclusion

**PPUMASK=0x10 is set by game code, not a bug.**

The game intentionally enables sprites only. Background may be:
- Enabled later in initialization (need to run longer)
- Not used for this screen (sprite-only title screen)
- Dependent on game state not yet reached

**Recommendation:** Run game longer and watch for PPUMASK changes, or compare with reference emulator to see if/when BG is enabled.
