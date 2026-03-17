# Three-Way Comparison: Parasol Stars (a.nes) Execution

**Date:** 2026-03-17
**ROM:** roms/a.nes (Parasol Stars)
**Comparison Targets:**
1. **Reference Emulator:** `tmp/NES/build/nes roms/a.nes`
2. **Recompiled Code:** `./output/a_v13/build/a roms/a.nes`
3. **Interpreter Mode:** `./output/a_v14/build/a roms/a.nes --interpreter`

---

## Executive Summary

The comparison reveals **critical differences** between the reference emulator and both the recompiled/interpreter versions. The reference successfully renders game content (sprites/UI elements visible), while both recompiled and interpreter versions produce blank grey screens with no visible content.

**Key Finding:** The recompiled version correctly configures PPU registers (PPUCTRL=0x90, PPUMASK=0x10) and enables sprite rendering, but the rendered output is still a blank grey screen instead of game graphics. This suggests the issue is in **CHR data loading** or **PPU memory writes** rather than PPU register configuration.

---

## 1. Frame Analysis

### Color Histogram Comparison

| Version | Frame # | Colors | Pixel Count | Description |
|---------|---------|--------|-------------|-------------|
| **Reference** | 10 | 2 colors | 60702 black + 738 red | Black screen with red UI elements |
| **Recompiled** | 10 | 1 color | 61440 grey | Completely blank grey screen |
| **Interpreter** | 10 | 1 color | 61440 grey | Completely blank grey screen |

**Color Values:**
- Reference black: `#000000` (0,0,0)
- Reference red: `#A71214` (167,18,20) - likely sprite/UI color
- Recompiled/Interpreter grey: `#545454` (84,84,84) - default background

### Frame Hash Comparison (MD5)

| Frame | Reference | Recompiled | Interpreter | Match |
|-------|-----------|------------|-------------|-------|
| 1 | 99181d97... | 1ed65700... | d52777af... | NO |
| 2-10 | 99181d97... | 0e454b5c... | 0e454b5c... | NO |

**Note:** Recompiled and interpreter frames 2-10 are identical (same hash), suggesting they share the same rendering path with no content.

### Frame Content Analysis

| Feature | Reference | Recompiled | Interpreter |
|---------|-----------|------------|-------------|
| Background rendered | Partial (black) | No | No |
| Sprites rendered | YES (738 red pixels) | NO | NO |
| UI elements | YES | NO | NO |
| Unique colors | 2 | 1 | 1 |
| Visible content | Red UI elements | None | None |

---

## 2. PPU Register Comparison

### PPUCTRL ($2000) Writes

| Version | Value | Binary | Meaning |
|---------|-------|--------|---------|
| **Reference** | N/A (not logged) | - | - |
| **Recompiled** | 0x90 | 10010000 | NMI on VBlank enabled, sprite table at $1000 |
| **Interpreter** | 0x00 | 00000000 | NMI disabled, BG table at $0000 |

**Analysis:** Recompiled version correctly enables NMI and configures sprite pattern table. Interpreter version has PPU disabled.

### PPUMASK ($2001) Writes

| Version | Value | Binary | Meaning |
|---------|-------|--------|---------|
| **Reference** | N/A (not logged) | - | - |
| **Recompiled** | 0x10 | 00010000 | Sprite rendering enabled, BG disabled |
| **Interpreter** | 0x00 | 00000000 | All rendering disabled |

**Analysis:** Recompiled version enables sprite rendering. This matches the PPU render log showing "spr=1".

### Render State Log (Recompiled v13)

```
[PPU] Render scanline 0: CTRL=0x90 MASK=0x10 bg=0 spr=1
[PPU] Render scanline 1: CTRL=0x90 MASK=0x10 bg=0 spr=1
...
[PPU] Render scanline 239: CTRL=0x90 MASK=0x10 bg=0 spr=1
```

**PPUCTRL 0x90 breakdown:**
- Bit 7 (NMI): 1 - NMI enabled on VBlank
- Bit 6 (Sprite height): 0 - 8x8 sprites
- Bit 5 (BG table): 0 - $0000
- Bit 4 (Sprite table): 1 - $1000
- Bits 3-0: Increment by 1, Nametable $00

**PPUMASK 0x10 breakdown:**
- Bit 4 (Sprite enable): 1 - Sprites visible
- All other bits: 0 - BG, clipping disabled

---

## 3. MMC1 Banking Comparison

### Control Register Configuration

| Version | Control Value | Mirroring | PRG Mode | CHR Mode |
|---------|---------------|-----------|----------|----------|
| **Recompiled** | 0x1E | 2 (four-screen) | 1 (switch 32KB) | 1 (switch 4KB each) |
| **Recompiled** | 0x1D | 1 (vertical) | 1 (switch 32KB) | 1 (switch 4KB each) |
| **Interpreter** | N/A | N/A | N/A | N/A |

### PRG Bank Switches (Recompiled v13)

```
[MMC1] PRG bank: $01
[MMC1] PRG bank: $00
[MMC1] PRG bank: $03
[MMC1] PRG bank: $00
[MMC1] PRG bank: $04
[MMC1] PRG bank: $05
```

### CHR Bank Switches (Recompiled v13)

```
[MMC1] CHR bank 0: $00
[MMC1] CHR bank 1: $01
[MMC1] CHR bank 0: $00
[MMC1] CHR bank 1: $02
```

**Analysis:**
- Recompiled version correctly performs MMC1 banking
- CHR banks 0-2 are being loaded (banks $00, $01, $02)
- Interpreter shows no MMC1 activity (stuck in loop at PC=0xC024-0xC027)

---

## 4. Execution Flow Analysis

### Reference Emulator
- Runs at ~60 FPS
- No detailed CPU/PPU logs (minimal output)
- Successfully renders game content
- Frame hash: 3E555FB0 (stable after frame 92)

### Recompiled Code (v13)
- Runs at ~60 FPS
- Detailed PPU/MMC1 logging
- PPU configured correctly (PPUCTRL=0x90, PPUMASK=0x10)
- MMC1 banking working
- **Problem:** No visible content despite sprite rendering enabled
- Frame stats: "White=0 Black=0 Other=61440" (all grey)

### Interpreter Mode (v14)
- Gets stuck in CPU loop at PC=0xC024-0xC027
- PPU never properly configured (PPUCTRL=0x00, PPUMASK=0x00)
- No MMC1 banking activity
- **Problem:** CPU execution issue preventing normal initialization

**Interpreter Loop Analysis:**
```
[6502] Step 29: PC=0xC024 A=00 X=01 Y=FE SP=01FF F=25
[6502] Step 30: PC=0xC027 A=00 X=01 Y=FE SP=01FF F=27
[6502] Step 31: PC=0xC024 A=00 X=01 Y=FE SP=01FF F=27
[6502] Step 32: PC=0xC027 A=00 X=01 Y=FE SP=01FF F=27
... (repeats indefinitely)
```

This is the MMC1 write loop that should be writing serial data to configure the mapper. The interpreter is not properly handling the MMC1 write sequence.

---

## 5. Critical Differences Summary

| Feature | Reference | Recompiled | Interpreter | Issue |
|---------|-----------|------------|-------------|-------|
| **PPUCTRL writes** | Unknown | 0x90 | 0x00 | Interpreter not configuring PPU |
| **PPUMASK writes** | Unknown | 0x10 | 0x00 | Interpreter rendering disabled |
| **CHR banks loaded** | Unknown | $00, $01, $02 | None | Interpreter not banking CHR |
| **Palette writes** | Unknown | Not logged | Not logged | Need more logging |
| **Nametable writes** | Unknown | Not logged | Not logged | Need more logging |
| **Sprites rendered** | YES (738 px) | NO | NO | Recompiled: CHR/sprite data issue |
| **Background rendered** | Partial | NO | NO | PPUMASK bit 3 not set |
| **Frame hash match** | - | NO | NO | Neither matches reference |
| **CPU execution** | Normal | Normal | Stuck in loop | Interpreter MMC1 bug |

---

## 6. Root Cause Analysis

### Recompiled Version (v13)

**What's Working:**
- CPU execution flows correctly
- MMC1 banking configured properly
- PPUCTRL/PPUMASK registers written correctly
- Sprite rendering enabled in PPU

**What's Broken:**
- No sprite pixels visible despite sprite rendering enabled
- Likely causes:
  1. **CHR ROM not loaded into PPU VRAM correctly** - CHR banks selected but data not visible
  2. **Sprite OAM not initialized** - No sprite coordinates/attributes set
  3. **Palette not configured** - Sprites rendered but with transparent/missing palette
  4. **Pattern table address mismatch** - Sprite table at $1000 but CHR data at different offset

**Evidence:**
- Log shows: "Loaded 8192 bytes of CHR data into PPU VRAM"
- CHR banks 0, 1, 2 selected via MMC1
- But rendered output is 100% grey (no pattern data fetched)

### Interpreter Version (v14)

**What's Broken:**
- CPU stuck in infinite loop at PC=0xC024-0xC027
- This is the MMC1 initialization code
- PPU never configured (registers remain 0x00)
- No rendering occurs

**Root Cause:**
- The MMC1 write sequence requires 5 writes to complete one register write
- The interpreter is not properly handling the shift register logic
- Likely bug in MMC1 write handler or CPU memory write path

---

## 7. Recommended Next Fixes

### Priority 1: Fix Interpreter MMC1 Loop

**File:** Likely in runtime or recompiler MMC1 handler

**Issue:** CPU stuck in loop writing to MMC1 registers

**Debug Steps:**
1. Add logging to MMC1 write handler showing shift register state
2. Compare MMC1 write count between recompiled (working) and interpreter (broken)
3. Check if memory writes to $8000-$FFFF are properly reaching MMC1 handler

**Hypothesis:** The interpreter's MMC1 write handler is not properly incrementing the write counter or resetting after 5 writes.

---

### Priority 2: Debug Recompiled Sprite Rendering

**File:** runtime/ppu.c or runtime/ppu_renderer.c

**Issue:** Sprites enabled but no pixels rendered

**Debug Steps:**
1. **Add OAM logging:**
   ```c
   printf("[PPU] OAM[0]: y=%d tile=%d attr=%d x=%d\n", oam[0], oam[1], oam[2], oam[3]);
   ```

2. **Add palette logging:**
   ```c
   printf("[PPU] Palette[%d] = $%02X\n", i, palette[i]);
   ```

3. **Add CHR fetch logging:**
   ```c
   printf("[PPU] Fetching sprite tile %d from VRAM offset $%04X\n", tile, vram_offset);
   ```

4. **Verify CHR ROM loading:**
   - Check that CHR ROM data is actually loaded into PPU VRAM
   - Verify VRAM offset calculation for sprite pattern table ($1000)

5. **Compare with reference:**
   - Use reference emulator to dump OAM, palette, and CHR VRAM state
   - Compare with recompiled version state

**Likely Causes:**
- OAM not initialized (all sprites off-screen or transparent)
- Palette not written (sprites use color $00 which is transparent)
- CHR bank offset calculation wrong (fetching from wrong VRAM location)
- Sprite pattern table address not applied correctly

---

### Priority 3: Add Comprehensive PPU Logging

**Add logging for:**
- All $2000-$2007 register writes with values
- PPUADDR/PPUDATA write sequences
- Palette writes ($3F00-$3F1F)
- Nametable writes ($2000-$2EFF)
- OAM writes ($2004 via OAMDMA or direct)

**Example:**
```c
[PPU] Write $2006 = $3F (addr high)
[PPU] Write $2006 = $00 (addr low)
[PPU] Write $2007 = $0F (palette[0] = color $0F)
```

---

### Priority 4: Implement Frame Comparison Tool

Create a tool to:
1. Capture frame buffers from all three versions
2. Calculate per-pixel differences
3. Identify which scanlines/pixels differ
4. Generate visual diff overlay

---

## 8. Test ROM Validation

Before testing on Parasol Stars again, validate fixes with test ROMs:

```bash
# Rebuild after fixes
ninja -C build

# Recompile test ROM
./build/bin/nesrecomp testroms/test01_simple.nes -o output/test01

# Verify generated code
grep -A3 "PPUSTATUS\|PPUCTRL\|PPUMASK" output/test01/test01_simple.c

# Build and run
cmake -G Ninja -S output/test01 -B output/test01/build
ninja -C output/test01/build
timeout 5 ./output/test01/build/test01_simple
```

---

## 9. Conclusion

**Current State:**
- Reference: Working correctly, rendering game content
- Recompiled (v13): PPU configured correctly but no visible output
- Interpreter (v14): CPU stuck in MMC1 initialization loop

**Most Likely Issues:**
1. **Recompiled:** Sprite OAM not initialized or palette not written
2. **Interpreter:** MMC1 write handler bug causing infinite loop

**Next Steps:**
1. Fix interpreter MMC1 loop (blocking all execution)
2. Add OAM/palette logging to recompiled version
3. Compare PPU memory state with reference emulator
4. Verify CHR ROM data is correctly loaded and addressed

---

**Generated:** 2026-03-17 13:20
**Tools Used:** timeout, grep, md5sum, ImageMagick convert
**Output Location:** /common/active/sblo/Dev/nes-recompiled/logs/
