# Task 002: 6502 Instruction Decoder

## Objective
Replace SM83 (GameBoy CPU) decoder with 6502 decoder.

## Background
6502 has 56 official opcodes + 54 unofficial opcodes.
Key differences from SM83:
- No bit rotation group (CB prefix)
- Different addressing modes (7 total)
- Different flag behavior
- No 16-bit registers (only A, X, Y, SP)

## Addressing Modes
1. Immediate (IMT): `LDA #$42`
2. Zero Page (ZPG): `LDA $42`
3. Zero Page X (ZPG_X): `LDA $42,X`
4. Zero Page Y (ZPG_Y): `LDA $42,Y`
5. Absolute (ABS): `LDA $4242`
6. Absolute X (ABS_X): `LDA $4242,X`
7. Absolute Y (ABS_Y): `LDA $4242,Y`
8. Indirect (IND): `JMP ($4242)`
9. Indexed Indirect (IDX_IND): `LDA ($42,X)`
10. Indirect Indexed (IND_IDX): `LDA ($42),Y`
11. Relative (REL): `BEQ label`
12. Implied (IMPL): `TAX`
13. Accumulator (ACC): `ROL A`

## Changes Required

### 1. Create `recompiler/include/recompiler/decoder_6502.h`
New decoder with:
- `Reg8` enum: A, X, Y (no B,C,D,E,H,L)
- `AddressMode` enum matching NES emulator
- `Instruction` struct with 6502-specific fields
- Cycle count tables (varies by addressing mode)

### 2. Create `recompiler/src/decoder_6502.cpp`
- Decode all 256 opcode slots
- Handle unofficial opcodes (mark as UNDEFINED initially)
- Reference: `/home/sblo/Ohjelmat/NES/src/cpu6502.h` instruction lookup table

### 3. Update `recompiler/src/main.cpp`
- Replace GB decoder calls with 6502 decoder

## Acceptance Criteria
- [ ] All official 56 opcodes decode correctly
- [ ] Disassembly output matches expected format
- [ ] Cycle counts match reference
- [ ] Test with known ROM bytes

## Dependencies
- Task 001 (NES Header)

## Estimated Effort
8-16 hours

## Reference
NES emulator lookup table at `cpu6502.h:instructionLookup[256]`
