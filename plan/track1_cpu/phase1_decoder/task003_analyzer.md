# Task 003: 6502 Control Flow Analyzer

## Objective
Adapt control flow analyzer for 6502 instruction set.

## Key Differences from GameBoy
- No bank switching in base NES (handled by mappers)
- Different interrupt vectors: NMI ($FFFA), Reset ($FFFC), IRQ ($FFFE)
- Stack is fixed at $0100-$01FF
- No condition codes - individual flags (N, V, Z, C)

## Changes Required

### 1. Modify `recompiler/include/recompiler/analyzer.h`
- Update `AnalysisResult` for NES memory map
- Change entry points to NES vectors ($FFFC for reset)
- Add interrupt vector handling (NMI, IRQ)

### 2. Modify `recompiler/src/analyzer.cpp`
- Update jump/call detection for 6502:
  - `JMP` (absolute, indirect)
  - `JSR` (call)
  - `RTS` (return)
  - `RTI` (return from interrupt)
  - Branch instructions (BCC, BCS, BEQ, BNE, BMI, BPL, BVC, BVS)
- Remove GameBoy-specific logic (HALT, STOP, bank switching)

### 3. Update `recompiler/src/bank_tracker.cpp`
- Simplify or remove for base NES (no banking)
- Prepare for mapper support (track 4)

## Acceptance Criteria
- [ ] Analyzer starts at $FFFC reset vector
- [ ] Follows JMP/JSR correctly
- [ ] Handles branch targets
- [ ] Identifies interrupt handlers

## Dependencies
- Task 002 (6502 Decoder)

## Estimated Effort
4-8 hours
