# Task 005: 6502 C Code Emitter

## Objective
Update C emitter to generate 6502-specific C code.

## Changes Required

### 1. Modify `recompiler/include/recompiler/codegen/c_emitter.h`
- Replace GB-specific methods with 6502 equivalents:
  - `emit_load_a_imm()`, `emit_load_a_addr()`, etc.
  - `emit_store_a_addr()`, `emit_store_x_addr()`, etc.
  - `emit_adc()`, `emit_sbc()` (6502 unique)
  - `emit_bit()` (6502-specific test)
  - `emit_branch_cc()` for 8 branch conditions

### 2. Modify `recompiler/src/codegen/c_emitter.cpp`
- Generate C code using nesrt runtime functions
- Handle 6502 flag semantics (especially V overflow flag)
- Generate proper function signatures

## Generated Code Pattern
```c
// JSR $C000
void func_00_8000(nes_ctx_t* ctx) {
    // ... code ...
    nes_call(ctx, 0xC000);
    return;
}

// LDA #$42
ctx->a = 0x42;
nes_set_flags_zn(ctx, ctx->a);

// ADC #$01
ctx->a = nes_adc(ctx, ctx->a, 0x01);
```

## Acceptance Criteria
- [ ] Generated C compiles without errors
- [ ] Flag handling correct
- [ ] Branch conditions work
- [ ] Stack operations correct

## Dependencies
- Task 004 (IR Builder)

## Estimated Effort
8-12 hours
