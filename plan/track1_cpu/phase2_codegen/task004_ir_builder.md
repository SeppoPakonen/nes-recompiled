# Task 004: 6502 IR Builder

## Objective
Update IR builder to emit 6502-specific intermediate representation.

## Changes Required

### 1. Modify `recompiler/include/recompiler/ir/ir.h`
- Update `InstructionType` enum for 6502 ops:
  - `LOAD_A_IMM`, `LOAD_A_ADDR`, `LOAD_A_X`, `LOAD_A_Y`
  - `STORE_A_ADDR`, `STORE_X_ADDR`, `STORE_Y_ADDR`
  - `TRANSFER_A_X`, `TRANSFER_A_Y`, `TRANSFER_X_A`, `TRANSFER_Y_A`
  - `TRANSFER_X_SP`, `TRANSFER_SP_X`
  - `PUSH_A`, `PUSH_SR`, `PULL_A`, `PULL_SR`
  - `ADD_CARRY`, `SUB_CARRY` (6502 has ADC/SBC only)
  - `AND`, `ORA`, `EOR`, `BIT`
  - `ASL`, `LSR`, `ROL`, `ROR`
  - `JMP_ABS`, `JMP_IND`, `JSR`, `RTS`, `RTI`
  - Branch ops: `BCC`, `BCS`, `BEQ`, `BNE`, `BMI`, `BPL`, `BVC`, `BVS`
  - `BRK`, `NOP`, `CLC`, `SEC`, `CLI`, `SEI`, `CLD`, `SED`, `CLV`, `PLA`, `PHA`

### 2. Modify `recompiler/src/ir/ir_builder.cpp`
- Map 6502 decoder output to IR
- Handle flag effects (N, V, Z, C, D, I)
- Address mode handling

## Acceptance Criteria
- [ ] All 6502 opcodes map to IR
- [ ] Flag effects tracked correctly
- [ ] Address modes handled properly

## Dependencies
- Task 002 (6502 Decoder)
- Task 003 (Analyzer)

## Estimated Effort
8-12 hours
