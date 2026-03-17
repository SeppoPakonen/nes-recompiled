# MMC1 Palette Bank Test

**Test ROM:** `testroms/test_mmc1_palette.nes`
**Build Script:** `testroms/build_test_mmc1_palette.py`

---

## Purpose

This test ROM reproduces the Parasol Stars palette initialization bug where:
- Palette init code exists at 0xB5EC in **Bank 0**
- Different code (SEC) exists at 0xB5EC in **Bank 1**
- The analyzer only found Bank 1, missing the palette code

---

## ROM Structure

| Bank | Address | Content |
|------|---------|---------|
| **Bank 0** | 0xB5EC | Palette initialization (`LDA #$3F; STA $2006; ...`) |
| **Bank 1** | 0xB5EC | SEC instruction only |
| **Bank 2** | 0xB5EC | NOP padding |
| **Bank 3** | 0xB5EC | NOP padding |
| **Fixed** | 0xC000 | Main code + vectors |

**ROM Size:** 64KB (4 × 16KB banks)
**Mapper:** MMC1 (mapper 1)

---

## Expected Behavior

1. **Analyzer should find BOTH functions:**
   - `func_00_b5ec` (palette init)
   - `func_01_b5ec` (SEC)

2. **Generated code should:**
   - Include both functions
   - Switch to correct bank before calling

3. **Runtime behavior:**
   - Main code switches to Bank 0
   - Calls 0xB5EC
   - Palette should be initialized
   - RAM $0002 should be set to $01

---

## Actual Behavior (Bug Reproduced)

```bash
./build/bin/nesrecomp testroms/test_mmc1_palette.nes -o output/test_mmc1_palette

# Generated functions:
grep "^static void func_" output/test_mmc1_palette/test_mmc1_palette.c
# Shows: func_01_b5ec, func_02_b5ec, func_03_b5ec
# MISSING: func_00_b5ec (palette init)!
```

**The analyzer found Bank 1, 2, 3 at 0xB5EC but NOT Bank 0!**

This is the **exact same bug** as Parasol Stars.

---

## Test Code

### Bank 0 at 0xB5EC (Palette Init)
```assembly
0xB5EC: A9 3F        LDA #$3F    ; PPUADDR high = $3F (palette)
0xB5EE: 8D 06 20     STA $2006
0xB5F1: A9 00        LDA #$00    ; PPUADDR low = $00
0xB5F3: 8D 06 20     STA $2006
0xB5F6: A9 0F        LDA #$0F    ; Black color
0xB5F8: 8D 07 20     STA $2007   ; Write to palette[0]
0xB5FB: A9 30        LDA #$30    ; White color
0xB5FD: 8D 07 20     STA $2007   ; Write to palette[1]
0xB600: A9 16        LDA #$16    ; Red color
0xB602: 8D 07 20     STA $2007   ; Write to palette[2]
0xB605: 60           RTS
```

### Bank 1 at 0xB5EC (SEC Only)
```assembly
0xB5EC: 38           SEC
0xB5ED: EA           NOP
0xB5EE: EA           NOP
...
```

### Fixed Bank (Main Code)
```assembly
0xC000: 78           SEI
0xC001: D8           CLD
...
; Switch MMC1 to Bank 0
0xC00A: A9 80        LDA #$80    ; Reset MMC1
0xC00C: 8D 00 80     STA $8000
...
0xC020: A9 00        LDA #$00    ; Write bank 0
0xC022: 8D 00 A0     STA $A000   ; To PRG bank register
...
; Call palette init at 0xB5EC (now in Bank 0)
0xC030: 20 EC B5     JSR $B5EC

; Success marker
0xC033: A9 01        LDA #$01
0xC035: 85 02        STA $0002   ; RAM $0002 = $01 if successful

; Infinite loop
0xC037: 4C 37 C0     JMP $C037
```

---

## How to Use

### Build Test ROM
```bash
cd testroms
python3 build_test_mmc1_palette.py
```

### Recompile
```bash
./build/bin/nesrecomp testroms/test_mmc1_palette.nes -o output/test_mmc1_palette
```

### Check Generated Code
```bash
# Should show func_00_b5ec (palette init)
grep "func_00_b5ec" output/test_mmc1_palette/test_mmc1_palette.c

# Check if palette code was generated
grep -A10 "loc_b5ec:" output/test_mmc1_palette/test_mmc1_palette.c | head -20
```

### Run (if runtime supported MMC1)
```bash
# Would execute and check RAM $0002
# $0002 = $01 if palette init was called
# $0002 = $00 if wrong bank was called
```

---

## Fix Verification

When the MMC1 bank tracking fix is complete:

1. **Analyzer output should show:**
   ```
   Found X functions
   - func_00_b5ec (Bank 0, palette init)
   - func_01_b5ec (Bank 1, SEC)
   - ...
   ```

2. **Generated code should include:**
   ```c
   static void func_00_b5ec(NESContext* ctx) {
       // Palette initialization code
       nes_write8(ctx, 0x2006, 0x3F);
       nes_write8(ctx, 0x2006, 0x00);
       nes_write8(ctx, 0x2007, 0x0F);
       // ...
   }
   ```

3. **Runtime should:**
   - Switch to Bank 0 before calling 0xB5EC
   - Execute palette init
   - Set RAM $0002 = $01

---

## Related Issues

- Parasol Stars palette bug (same root cause)
- MMC1 bank tracking in analyzer
- Multi-bank analysis performance issues

---

## Files

- `testroms/build_test_mmc1_palette.py` - Build script
- `testroms/test_mmc1_palette.nes` - Test ROM
- `output/test_mmc1_palette/` - Generated code
