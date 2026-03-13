# NES Feature Test ROMs

Comprehensive test ROMs that exercise every major NES hardware feature with stdout logging for verification.

## Directory Structure

```
testroms/
├── src/
│   └── features/
│       ├── test_cpu.asm       # CPU feature test (assembly reference)
│       ├── test_ppu.asm       # PPU feature test (assembly reference)
│       ├── test_apu.asm       # APU feature test (assembly reference)
│       ├── test_input.asm     # Controller input test (assembly reference)
│       └── test_timer.asm     # Timer feature test (assembly reference)
├── build_test_cpu.py          # CPU test ROM builder
├── build_test_ppu.py          # PPU test ROM builder
├── build_test_apu.py          # APU test ROM builder
├── build_test_input.py        # Input test ROM builder
├── build_test_timer.py        # Timer test ROM builder
├── build_features.sh          # Master build script
└── README_FEATURES.md         # This file
```

## Quick Start

```bash
# Build all feature test ROMs
cd testroms
./build_features.sh

# Build specific test
./build_features.sh cpu    # test_cpu.nes
./build_features.sh ppu    # test_ppu.nes
./build_features.sh apu    # test_apu.nes
./build_features.sh input  # test_input.nes
./build_features.sh timer  # test_timer.nes
```

## Debug Output Mechanism

All test ROMs write debug output to **$6000** (debug port). The runtime can intercept writes to this address and print the ASCII character to stdout.

Example output:
```
CPU TEST
LD OK
ARI OK
LOG OK
TRN OK
STK OK
INC OK
SHF OK
CMP OK
BRN OK
FLG OK
JSR OK
INT OK
DONE
```

## Test ROMs

### 1. test_cpu.nes - 6502 CPU Feature Test

**Purpose:** Test all 6502 instructions, addressing modes, flags, and interrupts.

**Features Tested:**
| Category | Instructions |
|----------|-------------|
| Load/Store | LDA, LDX, LDY, STA, STX, STY |
| Addressing Modes | Immediate, Zero Page, Absolute, Indexed, Indirect |
| Arithmetic | ADC, SBC, CLC, SEC |
| Logic | AND, ORA, EOR, BIT |
| Transfer | TAX, TAY, TXA, TYA, TSX, TXS |
| Stack | PHA, PLA, PHP, PLP |
| Modify | INC, DEC, INX, DEX, INY, DEY |
| Shift/Rotate | ASL, LSR, ROL, ROR |
| Compare | CMP, CPX, CPY |
| Branches | BEQ, BNE, BMI, BPL, BCC, BCS, BVC, BVS |
| Flags | CLC, SEC, CLI, SEI, CLV |
| Control | JMP, JSR, RTS, RTI |
| Interrupts | NMI, IRQ, BRK |

**Expected Output:**
```
CPU TEST
LD OK
ARI OK
LOG OK
TRN OK
STK OK
INC OK
SHF OK
CMP OK
BRN OK
FLG OK
JSR OK
INT OK
DONE
```

---

### 2. test_ppu.nes - PPU Feature Test

**Purpose:** Test PPU registers, VRAM, nametables, pattern tables, palettes, sprites, and VBlank.

**Features Tested:**
| Feature | Address/Range | Description |
|---------|---------------|-------------|
| PPUCTRL | $2000 | NMI enable, sprite size, BG/Sprite pattern table |
| PPUMASK | $2001 | BG/Sprite enable, grayscale, color emphasis |
| PPUSTATUS | $2002 | VBlank flag, sprite 0 hit |
| OAMADDR | $2003 | OAM address for sprite data |
| OAMDATA | $2004 | OAM data port |
| PPUSCROLL | $2005 | X/Y scroll registers |
| PPUADDR | $2006 | VRAM address register |
| PPUDATA | $2007 | VRAM data port |
| VRAM Write | $2000-$3FFF | Write to nametables, pattern tables, palettes |
| VRAM Read | $2000-$3FFF | Read from VRAM (with dummy read) |
| Nametable | $2000-$23FF | 960 bytes tile map |
| Pattern Table | $0000-$1FFF | 8x8 pixel tile graphics |
| Palette | $3F00-$3FFF | 32 bytes (4 BG + 4 sprite palettes × 4 colors) |
| Sprite OAM | $0200-$02FF | 64 sprites × 4 bytes (Y, tile#, attr, X) |
| VBlank | - | Wait for VBlank, NMI on VBlank |

**Expected Output:**
```
PPU TEST
REG OK
VW OK
VR OK
NAM OK
PAT OK
PAL OK
SPR OK
SCR OK
VBL OK
DONE
```

---

### 3. test_apu.nes - APU Feature Test

**Purpose:** Test all APU channels and audio registers.

**Features Tested:**
| Channel | Registers | Description |
|---------|-----------|-------------|
| Pulse 1 | $4000-$4003 | Control, sweep, timer (low/high) |
| Pulse 2 | $4004-$4007 | Control, sweep, timer (low/high) |
| Triangle | $4008-$400B | Control, timer (low/high) |
| Noise | $400C-$400F | Control, timer (low/high) |
| DMC | $4010-$4013 | Flags, raw DAC, start, length |
| Frame Counter | $4017 | 4-step/5-step sequence, IRQ |
| Status | $4015 | Channel enable/status |

**Pulse Channel Features:**
- Duty cycle (12.5%, 25%, 50%, 75%)
- Volume (0-15)
- Envelope (loop, disable)
- Sweep (enable, period, direction, shift)
- Timer (frequency control)

**Triangle Channel Features:**
- Linear counter (reload value, load mode)
- Timer (frequency control)

**Noise Channel Features:**
- Volume (0-15)
- Envelope
- Timer period index (0-15)
- Mode (LFSR, 7-bit LFSR)

**DMC Channel Features:**
- Frequency index (0-15)
- Loop mode
- IRQ enable
- Raw DAC (7-bit)
- Start address
- Sample length

**Expected Output:**
```
APU TEST
P1 OK
P2 OK
TRI OK
NOI OK
DMC OK
FRM OK
STS OK
DONE
```

---

### 4. test_input.nes - Controller Input Test

**Purpose:** Test controller reading via JOY1 and JOY2.

**Features Tested:**
| Feature | Register | Description |
|---------|----------|-------------|
| Strobe | $4016 (write) | Latch button states (write 1 then 0) |
| JOY1 Read | $4016 (read) | Read button states (shifts after each read) |
| JOY2 Read | $4017 (read) | Read controller 2 data |
| Shift Register | - | 8-button shift register |

**Button Mapping (bits 0-7):**
| Bit | Button |
|-----|--------|
| 0 | A |
| 1 | B |
| 2 | Select |
| 3 | Start |
| 4 | Up |
| 5 | Down |
| 6 | Left |
| 7 | Right |

**Reading Protocol:**
1. Write $01 to $4016 (strobe start)
2. Write $00 to $4016 (strobe end, begin reading)
3. Read $4016 eight times (one button per read, LSB first)
4. Additional reads return $01

**Expected Output:**
```
INPUT TEST
STR OK
BTN OK
SHF OK
2PD OK
MLT OK
DONE
```

---

### 5. test_timer.nes - Timer Feature Test

**Purpose:** Test timing functionality using CPU cycles, frame counter, and VBlank.

**Features Tested:**
| Feature | Method | Description |
|---------|--------|-------------|
| Cycle Counting | NOP, loops | Known cycle count instructions |
| Delay Loops | Nested loops | Short (~100), medium (~1000), long (~10000) cycles |
| Frame Counter | PPUSTATUS poll | 60 Hz frame detection |
| VBlank Timing | PPUSTATUS bit 7 | VBlank period (~2273 cycles) |
| NMI Timing | NMI handler | Precise 60 Hz interrupt timing |

**Timing Reference:**
- CPU clock: 1.789772 MHz (NTSC)
- Frame rate: 60.0988 Hz (NTSC)
- Cycles per frame: ~29780
- VBlank duration: ~2273 cycles (1.27 ms)
- 60 frames = ~1 second

**Expected Output:**
```
TIMER TEST
CYC OK
DLY OK
FRM OK
VBL OK
NMI OK
DONE
```

---

## Integration with Recompiler

To test the recompiler with feature test ROMs:

```bash
# Recompile a feature test ROM
./build/bin/nesrecomp testroms/test_cpu.nes -o output/test_cpu

# Build the recompiled code
cmake -G Ninja -S output/test_cpu -B output/test_cpu/build
ninja -C output/test_cpu/build

# Run and verify output
./output/test_cpu/build/test_cpu
```

## Verification Checklist

Use this checklist to verify each feature test:

### CPU Test Verification
- [ ] All load/store instructions work
- [ ] All addressing modes produce correct results
- [ ] Arithmetic operations set flags correctly
- [ ] Logic operations work (AND, ORA, EOR, BIT)
- [ ] Transfer instructions work (TAX, TAY, etc.)
- [ ] Stack operations work (PHA, PLA, PHP, PLP)
- [ ] Increment/decrement work (INC, DEC, INX, etc.)
- [ ] Shift/rotate work (ASL, LSR, ROL, ROR)
- [ ] Compare instructions set flags correctly
- [ ] All branches work (BEQ, BNE, BMI, etc.)
- [ ] Flag operations work (CLC, SEC, etc.)
- [ ] Jump/subroutine work (JMP, JSR, RTS)
- [ ] Interrupts are properly vectored

### PPU Test Verification
- [ ] PPU registers are writable
- [ ] VRAM writes work
- [ ] VRAM reads work (with dummy read)
- [ ] Nametable can be initialized
- [ ] Pattern table can be loaded
- [ ] Palette can be initialized
- [ ] Sprite OAM can be written
- [ ] Scroll registers work
- [ ] VBlank detection works
- [ ] NMI on VBlank works

### APU Test Verification
- [ ] Pulse 1 registers work
- [ ] Pulse 2 registers work
- [ ] Triangle registers work
- [ ] Noise registers work
- [ ] DMC registers work
- [ ] Frame counter modes work
- [ ] Status register works

### Input Test Verification
- [ ] Strobe signal works
- [ ] Button reading works
- [ ] Shift register operation works
- [ ] Controller 2 reading works
- [ ] Multiple read sequences work

### Timer Test Verification
- [ ] Cycle counting works
- [ ] Delay loops work
- [ ] Frame counter timing works
- [ ] VBlank timing works
- [ ] NMI timing works

## Troubleshooting

### Test hangs or doesn't complete
- Check for infinite loops in branch targets
- Verify interrupt vectors are set correctly
- Use `--trace` flag with recompiler to see where it stops

### Output doesn't match expected
- Verify debug port ($6000) is being intercepted
- Check flag operations for correct behavior
- Verify timing-sensitive tests have enough time

### Recompiler produces invalid code
- Use `--limit N` to stop after N instructions
- Check for data being interpreted as code
- Verify all opcodes are supported

## Adding New Feature Tests

1. Create assembly source in `src/features/test_<name>.asm`
2. Create Python builder `build_test_<name>.py`
3. Add test to `build_features.sh`
4. Update this README with test documentation
5. Add verification checklist items

## Resources

- [6502 Instruction Set Reference](https://www.masswerk.at/6502/6502_instruction_set.html)
- [NESDev Wiki](https://www.nesdev.org/wiki/)
- [NES APU Reference](https://www.nesdev.org/wiki/APU)
- [NES PPU Reference](https://www.nesdev.org/wiki/PPU)
- [NES Controller Reference](https://www.nesdev.org/wiki/Standard_controller)
