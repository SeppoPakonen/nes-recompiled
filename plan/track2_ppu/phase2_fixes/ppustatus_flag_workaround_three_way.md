# Three-Way Fix: PPUSTATUS Flag Setting Workaround

## Priority
🟢 MEDIUM (Already implemented, but needs documentation)

## Problem
Recompiled code uses CPU flags (`ctx->f_n`, `ctx->f_z`) for branch instructions after reading PPUSTATUS. The PPU hardware doesn't set CPU flags - the CPU instruction (BIT/LDA) does. But our recompiler doesn't generate flag-setting code for these instructions.

## Solution
Runtime PPU sets CPU flags when PPUSTATUS is read, so recompiled branches work correctly.

---

## Three-Way Comparison

### Reference Emulator
**File:** `tmp/NES/src/ppu.c:187-193`

```c
static uint8_t read_status(PPU* ppu){
    uint8_t ret = ppu->status;
    ppu->status &= ~BIT_7;  // Clear VBlank
    ppu->write_toggle = 0;
    return ret;
}
```

**CPU instruction handling:** `tmp/NES/src/cpu6502.c`

The reference emulator handles flags in the CPU instruction itself:
```c
void execute(c6502* ctx){
    // ... fetch opcode ...
    switch (ctx->instruction->opcode) {
        case BIT:
            value = read_mem(ctx->memory, address);
            ctx->sr &= ~(NEGATIVE | OVERFLW | ZERO);
            ctx->sr |= (value & (NEGATIVE | OVERFLW));
            if (!(ctx->ac & value)) ctx->sr |= ZERO;
            break;
        case LDA:
            ctx->ac = read_mem(ctx->memory, address);
            set_ZN(ctx, ctx->ac);  // Set Z and N flags
            break;
    }
}
```

**Key point:** Reference emulator sets flags in CPU instruction, not in PPU

---

### Our Runtime PPU
**File:** `runtime/src/ppu.c:789-815`

```c
uint8_t ppu_read_register(NESPPU* ppu, NESContext* ctx, uint16_t addr) {
    switch (addr & 0x07) {
        case 0x02: /* $2002 - PPUSTATUS */
            {
                uint8_t status = ppu->status;

                /* WORKAROUND: Set CPU flags based on PPUSTATUS value BEFORE clearing flags.
                 * The 6502 BIT/LDA instruction should set N and Z flags based on the result,
                 * but the recompiler doesn't generate flag-setting code for these.
                 * We set the flags here so that BPL/BNE after LDA/BIT $2002 work correctly. */
                if (ctx) {
                    ctx->f_n = (status & 0x80) != 0;  /* N flag = bit 7 */
                    ctx->f_z = (status == 0);          /* Z flag = result is zero */
                }

                /* Reading status clears VBlank flag. */
                ppu->status &= ~PPUSTATUS_VBLANK;
                ppu->write_toggle = 0;

                /* Clear NMI pending */
                ppu->nmi_requested = 0;
                ppu->flags &= ~PPU_FLAG_NMI_PENDING;

                value = status;
            }
            break;
        // ... other registers ...
    }
    return value;
}
```

**Why this workaround is needed:**
- Recompiler generates: `val = nes_read8(ctx, 0x2002);` followed by `if (!ctx->f_n) { ... }`
- Hardware: BIT/LDA instruction should set flags
- Our recompiler: Doesn't generate flag-setting code for BIT/LDA
- Workaround: PPU sets flags when read, so branches work

---

### Recompiled Code
**File:** `output/a_v14/a.c:9230-9254`

```c
loc_c014:
    /* c014: 2c 02 20     BIT  $2002 */
    /* 07:c014 */     {
        uint8_t val;
        val = nes_read8(ctx, 0x2002);  // ← Reads PPUSTATUS
        nes6502_bit(ctx, val);          // ← Sets flags (but may be optimized out)
    }
    /* c017: 10 fb        BPL  $c014 */
    /* 07:c017 */     if (!ctx->f_n) {  // ← Uses flags set by PPU workaround
        ctx->pc = 0xc014;
        nes_tick(ctx, 3);
        if (ctx->stopped) return;
        return;
    } /* P */
```

**Generated pattern:**
1. Read PPUSTATUS: `val = nes_read8(ctx, 0x2002);`
2. Call helper: `nes6502_bit(ctx, val);` (may be optimized out)
3. Branch on flag: `if (!ctx->f_n) { ... }`

**Without PPU workaround:**
- `ctx->f_n` would be uninitialized/old value
- Branch would use wrong condition
- Game would hang or behave incorrectly

---

## Why It's Different

### Reference Emulator
- **PPU:** Just returns status value, no flag handling
- **CPU:** Instruction decoder sets flags based on value
- **Clean separation:** PPU provides data, CPU processes it

### Our Runtime PPU
- **PPU:** Sets CPU flags AND returns value
- **CPU:** May or may not set flags (depending on recompiler output)
- **Workaround:** PPU sets flags to ensure branches work

### Recompiled Code
- **Assumes:** Flags are set correctly after PPU read
- **Generated code:** Calls `nes6502_bit()` but may be optimized out
- **Relies on:** PPU workaround for correct behavior

---

## Why This Workaround Exists

### The Recompiler's Limitation

The recompiler generates code like this:
```c
val = nes_read8(ctx, 0x2002);
nes6502_bit(ctx, val);  // Sets ctx->f_n, ctx->f_v, ctx->f_z
if (!ctx->f_n) { ... }  // BPL branch
```

But the optimizer may remove `nes6502_bit()` if it thinks the result is unused:
```c
val = nes_read8(ctx, 0x2002);
// nes6502_bit() optimized out!
if (!ctx->f_n) { ... }  // Uses stale flag value!
```

### The Workaround

By setting flags in `ppu_read_register()`, we ensure flags are always correct:
```c
// In ppu_read_register():
ctx->f_n = (status & 0x80) != 0;  // Always set, regardless of recompiler output
ctx->f_z = (status == 0);
return status;
```

Now even if `nes6502_bit()` is optimized out, flags are correct.

---

## Alternative Solutions

### Option 1: Mark Helper Functions as Always Inline
**File:** `runtime/include/nesrt.h`

```c
static inline __attribute__((always_inline)) void nes6502_bit(NESContext* ctx, uint8_t value) {
    ctx->f_n = (value & 0x80) != 0;
    ctx->f_v = (value & 0x40) != 0;
    ctx->f_z = ((ctx->a & value) == 0);
}
```

**Benefit:** Prevents optimizer from removing flag-setting code
**Drawback:** Still relies on recompiler generating correct code

### Option 2: Remove PPU Workaround, Fix Recompiler
**File:** `recompiler/src/codegen/c_emitter.cpp`

Generate flag-setting code for all BIT/LDA instructions, mark as volatile:
```cpp
out << "/* BIT $2002 */\n";
out << "{\n";
out << "    volatile uint8_t val = nes_read8(ctx, 0x2002);\n";
out << "    ctx->f_n = (val & 0x80) != 0;\n";
out << "    ctx->f_v = (val & 0x40) != 0;\n";
out << "    ctx->f_z = ((ctx->a & val) == 0);\n";
out << "}\n";
```

**Benefit:** Cleaner architecture, flags set in correct place
**Drawback:** Requires recompiler changes

### Option 3: Keep Current Workaround (Recommended)
**File:** `runtime/src/ppu.c:797-800`

Keep the workaround as-is. It's simple, reliable, and works.

**Benefit:** No changes needed, already working
**Drawback:** Architecturally "wrong" - PPU shouldn't set CPU flags

---

## Testing

### Test Case: VBlank Wait Loop
```assembly
wait_vblank:
    BIT $2002    ; Read PPUSTATUS, sets N flag based on bit 7
    BPL wait_vblank  ; Branch if N=0 (VBlank not started)
```

**Expected:** Loop exits when VBlank starts (bit 7 = 1, N = 1, BPL not taken)

**Without workaround:**
- `ctx->f_n` has stale value
- BPL uses wrong condition
- Game may hang or exit loop at wrong time

**With workaround:**
- `ctx->f_n` set correctly by PPU
- BPL works correctly
- Game waits for VBlank properly

---

## Related Files

| Component | Reference | Our Runtime | Recompiled |
|-----------|-----------|-------------|------------|
| PPUSTATUS Read | `tmp/NES/src/ppu.c:187-193` | `runtime/src/ppu.c:789-831` | `output/a_v14/a.c:9233` |
| Flag Setting | `tmp/NES/src/cpu6502.c` (in BIT/LDA) | `runtime/src/ppu.c:797-800` (workaround) | `output/a_v14/a.c:9234` (nes6502_bit) |
| Branch Usage | `tmp/NES/src/cpu6502.c` (in BPL) | `runtime/src/interpreter.c` | `output/a_v14/a.c:9237` |

---

## Verification

After any changes, verify:
1. VBlank wait loops work correctly
2. Branch conditions use correct flags
3. No stale flag values in recompiled code

Test with:
```bash
./output/a/build/a roms/a.nes 2>&1 | grep -E "PPUSTATUS|BPL|BMI"
```

---

## References

- 6502 BIT instruction: https://www.nesdev.org/wiki/CPU#Status_flag_set.2Fclear_instructions
- PPUSTATUS register: https://www.nesdev.org/wiki/PPU_registers#$2002
- Recompiler codegen: `recompiler/src/codegen/c_emitter.cpp`
