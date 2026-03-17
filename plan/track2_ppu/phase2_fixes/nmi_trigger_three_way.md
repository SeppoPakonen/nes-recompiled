# Three-Way Fix: NMI Triggering

## Priority
🔴 CRITICAL

## Problem
VBlank NMI is not triggered in the CPU when VBlank starts. Games relying on VBlank NMI for timing will not work correctly.

## Impact
- Games using VBlank NMI will poll PPUSTATUS forever
- Initialization loops that wait for NMI will never complete
- Most NES games use VBlank NMI for frame timing

---

## Three-Way Comparison

### Reference Emulator
**File:** `tmp/NES/src/ppu.c:204-209`

```c
static void update_NMI(PPU* ppu) {
    if(ppu->ctrl & BIT_7 && ppu->status & BIT_7)
        interrupt(&ppu->emulator->cpu, NMI);  // ← Directly triggers CPU NMI
    else
        interrupt_clear(&ppu->emulator->cpu, NMI);
}
```

**Called from:** `tmp/NES/src/ppu.c:292` (when VBlank starts)

```c
if(ppu->scanlines == VBLANK_SCANLINE && ppu->dots == 1){
    ppu->status |= V_BLANK;
    update_NMI(ppu);  // ← Trigger NMI
}
```

**CPU interrupt function:** `tmp/NES/src/cpu6502.c`

```c
void interrupt(c6502* ctx, Interrupt code) {
    ctx->interrupt |= code;
    ctx->state |= INTERRUPT_PENDING;
}
```

---

### Our Runtime PPU
**File:** `runtime/src/ppu.c:577-589`

```c
static void update_vblank_nmi(NESPPU* ppu, NESContext* ctx) {
    if ((ppu->status & PPUSTATUS_VBLANK) && (ppu->ctrl & PPUCTRL_VBLANK_INT)) {
        if (!ppu->nmi_requested) {
            ppu->nmi_requested = 1;
            ppu->flags |= PPU_FLAG_NMI_PENDING;
            
            /* Trigger NMI in CPU */
            if (ctx) {
                ctx->ime = 1;  /* Enable interrupts */
                /* ← MISSING: Actually trigger NMI interrupt! */
            }
        }
    }
}
```

**Called from:** `runtime/src/ppu.c:616` (when VBlank starts)

```c
if (ppu->scanline == PPU_VBLANK_SCANLINE && ppu->cycle == 0) {
    ppu->status |= PPUSTATUS_VBLANK;
    update_vblank_nmi(ppu, ctx);  // ← Sets flag but doesn't trigger
}
```

**Missing:** Function to trigger 6502 NMI in CPU context

---

### Recompiled Code
**File:** `output/a_v14/a.c` (generated)

NMI handling is not directly visible in recompiled code - it's handled by the runtime.

However, recompiled code assumes NMI works:
- Games set up NMI vector at `$FFFA`
- Games enable NMI via `STA $2000` (PPUCTRL bit 7)
- Games expect NMI to be triggered at VBlank

**Example pattern in recompiled code:**
```c
// Game sets up NMI
func_07_c000:
    LDA #$80      ; Enable NMI
    STA $2000     ; PPUCTRL
    // ... wait for NMI ...
```

---

## Why It's Different

### Reference Emulator
- **PPU directly calls CPU interrupt function**
- CPU has `interrupt()` and `interrupt_clear()` functions
- NMI is queued and handled at appropriate cycle boundary

### Our Runtime PPU
- **PPU only sets flags, doesn't trigger CPU interrupt**
- No equivalent `nes_trigger_nmi()` function exists
- `ctx->ime = 1` enables interrupts but doesn't trigger NMI

### Recompiled Code
- **Assumes NMI works correctly**
- No NMI handling code generated - relies on runtime
- Games will hang waiting for NMI that never comes

---

## Fix Implementation

### Step 1: Add NMI Trigger Function to Runtime
**File:** `runtime/src/nesrt.c` (add new function)

```c
/**
 * @brief Trigger NMI interrupt in 6502 CPU
 * @param ctx NES context
 */
void nes_trigger_nmi(NESContext* ctx) {
    if (!ctx) return;
    
    /* 6502 NMI sequence:
     * 1. Push PC (high byte first)
     * 2. Push status register (with B flag clear)
     * 3. Set I flag (disable interrupts)
     * 4. Load PC from NMI vector ($FFFA)
     */
    
    /* Push PC high byte */
    ctx->sp--;
    nes_write8(ctx, 0x0100 | ctx->sp, ctx->pc >> 8);
    
    /* Push PC low byte */
    ctx->sp--;
    nes_write8(ctx, 0x0100 | ctx->sp, ctx->pc & 0xFF);
    
    /* Push status register (B flag clear, I flag set) */
    uint8_t status = (ctx->f_n ? 0x80 : 0) |
                     (ctx->f_v ? 0x40 : 0) |
                     0x20 |  /* Unused bit, always 1 */
                     (ctx->f_i ? 0x04 : 0) |
                     0x04 |  /* I flag set (disable interrupts) */
                     (ctx->f_z ? 0x02 : 0) |
                     (ctx->f_c ? 0x01 : 0);
    ctx->sp--;
    nes_write8(ctx, 0x0100 | ctx->sp, status);
    
    /* Set I flag in context */
    ctx->f_i = 1;
    
    /* Load PC from NMI vector */
    ctx->pc = nes_read16(ctx, 0xFFFA);
    
    DBG_GENERAL("NMI triggered, PC=0x%04X", ctx->pc);
}
```

### Step 2: Update PPU to Call NMI Trigger
**File:** `runtime/src/ppu.c:577-589` (modify `update_vblank_nmi`)

```c
static void update_vblank_nmi(NESPPU* ppu, NESContext* ctx) {
    if ((ppu->status & PPUSTATUS_VBLANK) && (ppu->ctrl & PPUCTRL_VBLANK_INT)) {
        if (!ppu->nmi_requested) {
            ppu->nmi_requested = 1;
            ppu->flags |= PPU_FLAG_NMI_PENDING;
            
            /* Trigger NMI in CPU */
            if (ctx) {
                nes_trigger_nmi(ctx);  // ← NEW: Actually trigger NMI
            }
        }
    } else {
        /* NMI condition not met - clear pending */
        ppu->nmi_requested = 0;
        ppu->flags &= ~PPU_FLAG_NMI_PENDING;
    }
}
```

### Step 3: Add Declaration to Header
**File:** `runtime/include/nesrt.h` (add declaration)

```c
/**
 * @brief Trigger NMI interrupt in 6502 CPU
 * @param ctx NES context
 */
void nes_trigger_nmi(NESContext* ctx);
```

---

## Testing

### Test Case 1: Simple NMI Wait
```assembly
; Set up NMI vector
LDA #<nmi_handler
STA $FFFA
LDA #>nmi_handler
STA $FFFB

; Enable NMI
LDA #$80
STA $2000

; Wait for NMI (should complete quickly)
wait_nmi:
    JMP wait_nmi

nmi_handler:
    RTI
```

**Expected:** Game should enter NMI handler within one frame
**Current:** Game hangs forever in `wait_nmi` loop
**After fix:** Game should enter NMI handler

### Test Case 2: Parasol Stars
**Expected:** Game should initialize and show title screen
**Current:** Stuck in initialization loop (takes hours)
**After fix:** Should initialize in seconds

---

## Related Files

| Component | Reference | Our Runtime | Recompiled |
|-----------|-----------|-------------|------------|
| NMI Trigger | `tmp/NES/src/ppu.c:204-209` | `runtime/src/ppu.c:577-589` (broken) | N/A |
| CPU Interrupt | `tmp/NES/src/cpu6502.c` | `runtime/src/nesrt.c` (add new) | N/A |
| NMI Vector | Hardware $FFFA | Hardware $FFFA | Generated in ROM |

---

## Verification

After implementing fix:
1. Run interpreter mode with NMI-waiting game
2. Check that `nes_trigger_nmi()` is called at VBlank
3. Verify game enters NMI handler within one frame
4. Check execution trace for NMI vector load from $FFFA

---

## References

- 6502 NMI behavior: https://www.nesdev.org/wiki/Non-maskable_interrupt
- Reference implementation: `tmp/NES/src/ppu.c:204-209`
- 6502 interrupt sequence: https://www.nesdev.org/wiki/CPU_interrupts
