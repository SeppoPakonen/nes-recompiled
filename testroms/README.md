# NES Test ROMs

This directory contains 6502 assembly test programs for testing and validating the NES recompiler.

## Purpose

These test ROMs are designed to:
1. Verify the recompiler correctly translates 6502 assembly to C
2. Test specific instruction categories in isolation
3. Validate addressing modes
4. Catch regressions in the code generator

## Directory Structure

```
testroms/
├── src/                        # Assembly source files (reference)
│   ├── test01_simple.asm       # Basic instructions
│   ├── test02_transfers.asm    # Transfer, stack, modify instructions
│   └── test03_addressing.asm   # All addressing modes
├── build/                      # Build artifacts (intermediate .o files)
├── build.sh                    # Build script for all ROMs
├── build_test01.py             # Python builder for test01
├── build_test02.py             # Python builder for test02
├── build_test03.py             # Python builder for test03
├── test01_simple.nes           # Generated ROM (16KB)
├── test02_transfers.nes        # Generated ROM (16KB)
└── test03_addressing.nes       # Generated ROM (16KB)
```

## Building

### Prerequisites
- Python 3.6+
- No additional dependencies (uses only standard library)

### Build Commands

```bash
# Build all test ROMs
cd testroms
./build.sh

# Build specific ROM
./build.sh 01        # test01_simple.nes
./build.sh 02        # test02_transfers.nes
./build.sh 03        # test03_addressing.nes

# Or run Python builder directly
python3 build_test01.py
```

## Test ROMs

### test01_simple.nes

**Purpose:** Basic 6502 instructions and control flow

**Size:** 16,400 bytes (16KB PRG ROM)

**Instructions covered:**
| Category | Instructions |
|----------|-------------|
| Load/Store | LDA, LDX, LDY, STA, STX, STY, TXS |
| Arithmetic | ADC, SBC, CLC, SEC |
| Logic | AND, ORA, EOR |
| Control Flow | JMP, JSR, RTS |
| Branches | BEQ, BNE, BMI, BPL |

**Test pattern:**
1. Initialize stack pointer (`LDX #$FF`, `TXS`)
2. Store test values to zero page
3. Perform arithmetic operations (ADC, SBC)
4. Perform logic operations (AND, ORA, EOR)
5. Test all branch conditions (BEQ, BNE, BMI, BPL)
6. Call subroutine and return (JSR/RTS)
7. Enter infinite loop

**Key test addresses:**
- `$0000-$000E`: Zero page test data
- `$8000`: Reset vector entry point
- `$8039`: BEQ branch test
- `$8041`: BNE branch test
- `$8061`: Subroutine with RTS

---

### test02_transfers.nes

**Purpose:** Transfer, stack, and modify instructions

**Size:** 16,400 bytes (16KB PRG ROM)

**Instructions covered:**
| Category | Instructions |
|----------|-------------|
| Transfer | TAX, TAY, TXA, TYA |
| Stack | PHA, PLA |
| Increment/Decrement | INC, DEC, INX, INY, DEX, DEY |
| Shifts/Rotates | ASL, LSR, ROL, ROR |
| Compare | CMP, CPX, CPY |
| Flags | CLC, SEC, CLI, SEI, CLV |
| Test | BIT |
| Branches | BCC, BCS, BVC, BVS |

**Test pattern:**
1. Transfer operations between registers
2. Stack push/pull operations
3. Memory increment/decrement
4. Shift and rotate operations
5. Compare operations with branches
6. Flag manipulation

**Key test addresses:**
- `$8000`: Reset vector entry
- `$8005`: TAX/TAY tests
- `$800B`: PHA/PLA tests
- `$8015`: INC/DEC memory tests
- `$8020`: ASL/LSR/ROL/ROR tests

---

### test03_addressing.nes

**Purpose:** All 6502 addressing modes

**Size:** 16,400 bytes (16KB PRG ROM)

**Addressing modes covered:**
| Mode | Example | Description |
|------|---------|-------------|
| Immediate | `LDA #$42` | Operand follows opcode |
| Zero Page | `LDA $42` | 8-bit address, faster |
| Zero Page,X | `LDA $42,X` | Zero page + X index |
| Zero Page,Y | `LDA $42,Y` | Zero page + Y index |
| Absolute | `LDA $1234` | 16-bit address |
| Absolute,X | `LDA $1234,X` | Absolute + X index |
| Absolute,Y | `LDA $1234,Y` | Absolute + Y index |
| Indirect,X | `LDA ($42,X)` | Indexed indirect |
| Indirect,Y | `LDA ($42),Y` | Indirect indexed |
| Implied | `CLC` | No operand |
| Accumulator | `ASL A` | Operates on A |

**Test pattern:**
1. Initialize test data at various addresses
2. Test each addressing mode with LDA/STA
3. Verify correct memory access
4. Test indexed addressing with X and Y

**Key test addresses:**
- `$0000-$00FF`: Zero page test data
- `$0100-$01FF`: Test data for indexed modes
- `$8000`: Reset vector entry
- `$8010`: Zero page tests
- `$8020`: Absolute tests
- `$8030`: Indexed tests
- `$8050`: Indirect tests

## Recompiler Testing Workflow

### 1. Recompile Test ROM

```bash
./build/bin/nesrecomp testroms/test01_simple.nes -o output/test01
```

### 2. Examine Generated C Code

```bash
# Check branch conditions
grep -A3 "BEQ\|BNE" output/test01/test01_simple.c

# Check RTS implementation
grep -A2 "RTS" output/test01/test01_simple.c

# Check INC/DEC memory operations
grep -A5 "INC" output/test01/test01_simple.c
```

### 3. Build Generated Code

```bash
cmake -G Ninja -S output/test01 -B output/test01/build
ninja -C output/test01/build
```

### 4. Run and Verify

```bash
timeout 5 ./output/test01/build/test01_simple 2>&1 | head -30
```

### 5. Compare with Original Assembly

Open both files side by side:
- `testroms/src/test01_simple.asm`
- `output/test01/test01_simple.c`

Verify each assembly instruction has correct C equivalent.

## Known Issues and Fixes

### Fixed Issues

| Issue | Status | Fix Commit |
|-------|--------|------------|
| Branch conditions check wrong flag | ✅ Fixed | 71aac92 |
| RTS doesn't pop return address | ✅ Fixed | 71aac92 |
| INC/DEC uses accumulator instead of memory | ✅ Fixed | 71aac92 |

### Remaining Issues

| Issue | Priority | Notes |
|-------|----------|-------|
| PHA/PLA use 16-bit stack ops | Medium | May cause stack corruption |
| Indirect,Y not implemented | High | Only affects test03 |
| Disassembler comment bugs | Low | Comments wrong, code correct |

## iNES Format

All ROMs use the iNES format (NES 1.0):

```
Bytes 0-3:   Magic "NES\x1A"
Byte 4:      PRG ROM size (in 16KB units)
Byte 5:      CHR ROM size (in 8KB units)
Byte 6:      Flags 6 (mapper low, mirroring, battery, trainer)
Byte 7:      Flags 7 (mapper high, VS/Playchoice, NES 2.0)
Bytes 8-15:  Reserved (zeros)
Bytes 16-:   PRG ROM data
Last 6 bytes: Interrupt vectors (NMI, Reset, IRQ)
```

**Our test ROMs:**
- PRG ROM: 1 x 16KB = 16KB
- CHR ROM: 0 x 8KB = 0KB (no graphics)
- Mapper: 0 (NROM)
- Mirroring: Vertical

## Adding New Test ROMs

1. **Create assembly source** in `src/testXX_description.asm`
   - Use existing ROMs as templates
   - Include iNES header (16 bytes)
   - Include interrupt vectors at end

2. **Create Python builder** `build_testXX.py`:
   ```python
   #!/usr/bin/env python3
   # Parse assembly, generate binary, add iNES header
   ```

3. **Add to build.sh**:
   ```bash
   ./build_testXX.py
   ```

4. **Test with recompiler**:
   ```bash
   ./build/bin/nesrecomp testroms/testXX_description.nes -o output/testXX
   ```

## 6502 Opcode Reference

### Load/Store Instructions

| Opcode | Instruction | Description | C Equivalent |
|--------|-------------|-------------|--------------|
| A9 | LDA #imm | Load Accumulator | `ctx->a = imm;` |
| A2 | LDX #imm | Load X Register | `ctx->x = imm;` |
| A0 | LDY #imm | Load Y Register | `ctx->y = imm;` |
| 85 | STA zp | Store Accumulator | `nes_write8(ctx, zp, ctx->a);` |
| 86 | STX zp | Store X Register | `nes_write8(ctx, zp, ctx->x);` |
| 84 | STY zp | Store Y Register | `nes_write8(ctx, zp, ctx->y);` |

### Arithmetic Instructions

| Opcode | Instruction | Description | C Equivalent |
|--------|-------------|-------------|--------------|
| 69 | ADC #imm | Add with Carry | `ctx->a = nes_adc(ctx, ctx->a, imm);` |
| E9 | SBC #imm | Subtract with Carry | `ctx->a = nes_sbc(ctx, ctx->a, imm);` |
| 18 | CLC | Clear Carry | `ctx->f_c = 0;` |
| 38 | SEC | Set Carry | `ctx->f_c = 1;` |

### Logic Instructions

| Opcode | Instruction | Description | C Equivalent |
|--------|-------------|-------------|--------------|
| 29 | AND #imm | Logical AND | `ctx->a &= imm;` |
| 09 | ORA #imm | Logical OR | `ctx->a |= imm;` |
| 49 | EOR #imm | Logical XOR | `ctx->a ^= imm;` |

### Control Flow Instructions

| Opcode | Instruction | Description | C Equivalent |
|--------|-------------|-------------|--------------|
| 4C | JMP addr | Jump | `ctx->pc = addr; return;` |
| 20 | JSR addr | Jump to Subroutine | `nes_call(ctx, addr); return;` |
| 60 | RTS | Return from Subroutine | `ctx->pc = nes_pop16(ctx) + 1; return;` |
| F0 | BEQ rel | Branch if Equal | `if (ctx->f_z) { branch... }` |
| D0 | BNE rel | Branch if Not Equal | `if (!ctx->f_z) { branch... }` |
| 30 | BMI rel | Branch if Minus | `if (ctx->f_n) { branch... }` |
| 10 | BPL rel | Branch if Plus | `if (!ctx->f_n) { branch... }` |

### Transfer Instructions

| Opcode | Instruction | Description | C Equivalent |
|--------|-------------|-------------|--------------|
| AA | TAX | Transfer A to X | `ctx->x = ctx->a;` |
| A8 | TAY | Transfer A to Y | `ctx->y = ctx->a;` |
| 8A | TXA | Transfer X to A | `ctx->a = ctx->x;` |
| 98 | TYA | Transfer Y to A | `ctx->a = ctx->y;` |
| 48 | PHA | Push Accumulator | `nes_push8(ctx, ctx->a);` |
| 68 | PLA | Pull Accumulator | `ctx->a = nes_pop8(ctx);` |

### Modify Instructions

| Opcode | Instruction | Description | C Equivalent |
|--------|-------------|-------------|--------------|
| E6 | INC zp | Increment Memory | `val = read(zp); val++; write(zp, val);` |
| C6 | DEC zp | Decrement Memory | `val = read(zp); val--; write(zp, val);` |
| E8 | INX | Increment X | `ctx->x++;` |
| C8 | INY | Increment Y | `ctx->y++;` |
| CA | DEX | Decrement X | `ctx->x--;` |
| 88 | DEY | Decrement Y | `ctx->y--;` |

## Resources

- [6502 Instruction Set Reference](https://www.masswerk.at/6502/6502_instruction_set.html)
- [NESDev Wiki](https://www.nesdev.org/wiki/)
- [iNES File Format](https://www.nesdev.org/wiki/iNES)
