# Interpreter Mode Fix Plan

## Goal
Make `--interpreter` mode work correctly for NES ROMs by implementing a proper 6502 CPU interpreter.

## Current Status
- **Problem**: The `nes_interpret()` function uses GameBoy (SM83) CPU interpreter, not 6502
- **Evidence**: Register dump shows GameBoy registers (A, B, C, D, E, H, L) instead of 6502 (A, X, Y, SP)
- **Reference**: 6502 interpreter available at `tmp/NES/src/cpu6502.c`

## Tasks

### Phase 1: CPU Core Integration
- [ ] **1.1** Analyze reference 6502 interpreter structure
  - [ ] Read `tmp/NES/src/cpu6502.c` and `tmp/NES/src/cpu6502.h`
  - [ ] Identify dependencies (mmu.h, ppu.h, etc.)
  - [ ] Document CPU state structure (c6502)

- [ ] **1.2** Create 6502 interpreter wrapper for runtime
  - [ ] Create `runtime/src/cpu6502_interp.c` with clean API
  - [ ] Define `nes6502_step()` function that executes one instruction
  - [ ] Define `nes6502_init()` for initialization
  - [ ] Define `nes6502_reset()` for reset

- [ ] **1.3** Integrate with NESContext
  - [ ] Add `c6502 cpu` field to `NESContext` struct
  - [ ] Initialize CPU on context creation
  - [ ] Connect CPU to memory (wram, PPU, mapper)

- [ ] **1.4** Replace GameBoy interpreter calls
  - [ ] Modify `nes_interpret()` to call 6502 interpreter
  - [ ] Update interpreter mode in main loop

### Phase 2: Memory Interface
- [ ] **2.1** Implement memory read/write callbacks
  - [ ] `cpu_read8(addr)` - Read byte from CPU address space
  - [ ] `cpu_write8(addr, value)` - Write byte
  - [ ] Connect to existing `nes_read8/nes_write8`

- [ ] **2.2** Implement stack operations
  - [ ] Ensure stack stays in $0100-$01FF range
  - [ ] Test PHA/PLA/PHP/PLP/JSR/RTS

- [ ] **2.3** Implement zero page addressing
  - [ ] Verify $0000-$00FF accesses work correctly

### Phase 3: Interrupt Handling
- [ ] **3.1** Implement NMI handling
  - [ ] Connect to PPU VBlank NMI
  - [ ] Test NMI vector at $FFFA

- [ ] **3.2** Implement IRQ handling
  - [ ] Connect to APU frame IRQ
  - [ ] Test IRQ vector at $FFFE

- [ ] **3.3** Implement reset
  - [ ] Test reset vector at $FFFC

### Phase 4: Testing & Debugging
- [ ] **4.1** Create CPU test harness
  - [ ] Load test ROM
  - [ ] Step through instructions
  - [ ] Compare with reference emulator

- [ ] **4.2** Test basic instructions
  - [ ] LDA/STA (load/store)
  - [ ] ADD/ADC (arithmetic)
  - [ ] JMP/JSR/RTS (control flow)
  - [ ] Branch instructions (BEQ, BNE, etc.)

- [ ] **4.3** Test with Parasol Stars
  - [ ] Run in interpreter mode
  - [ ] Compare execution with recompiled code
  - [ ] Verify PPU initialization

### Phase 5: Comparison Mode (Optional)
- [ ] **5.1** Add `--compare` mode
  - [ ] Run both interpreter and recompiled in parallel
  - [ ] Compare memory/registers after each instruction
  - [ ] Break on first difference

- [ ] **5.2** Add comparison logging
  - [ ] Log differences to file
  - [ ] Show side-by-side comparison

## Files to Create/Modify

### New Files
- `runtime/src/cpu6502_interp.c` - 6502 interpreter wrapper
- `runtime/include/cpu6502_interp.h` - Header

### Modified Files
- `runtime/include/nesrt.h` - Add CPU to NESContext
- `runtime/src/nesrt.c` - Initialize CPU, connect memory
- `runtime/src/interpreter.c` - Replace GameBoy code with 6502
- `recompiler/src/codegen/c_emitter.cpp` - Add --compare option

### Reference Files (from tmp/NES/)
- `tmp/NES/src/cpu6502.c` - Reference 6502 implementation
- `tmp/NES/src/cpu6502.h` - CPU structure and API
- `tmp/NES/src/mmu.c` - Memory interface
- `tmp/NES/src/ppu.c` - PPU interface

## Success Criteria
1. `./output/a/build/a roms/a.nes --interpreter` runs without crashing
2. Interpreter execution matches recompiled code execution
3. PPU initialization completes (PPUCTRL/PPUMASK writes)
4. Game renders graphics (not solid gray)

## Timeline Estimate
- Phase 1: 2-3 days
- Phase 2: 1-2 days
- Phase 3: 1-2 days
- Phase 4: 2-3 days
- Phase 5: 1-2 days (optional)

**Total: 7-12 days**

## Notes
- Reference emulator at `tmp/NES/` has working 6502 interpreter
- Key difference: 6502 uses stack at $0100-$01FF (not GameBoy's full stack)
- 6502 flags: N, V, B, D, I, Z, C (different from GameBoy)
- 6502 registers: A, X, Y, SP, PC (vs GameBoy's A, B, C, D, E, H, L)
