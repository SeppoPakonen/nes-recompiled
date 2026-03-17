# Three-Way Fix: Interpreter Speed Optimization

## Priority
🟠 HIGH

## Problem
Interpreter mode is extremely slow - games take hours to initialize. Reference emulator runs at full speed.

## Root Cause
Per-instruction PPU synchronization with expensive rendering loop in `ppu_tick()`.

---

## Three-Way Comparison

### Reference Emulator
**File:** `tmp/NES/src/emulator.c:155-176`

```c
while (!ppu->render) {
    execute_ppu(ppu);    // 1st PPU tick - simple state machine
    execute_ppu(ppu);    // 2nd PPU tick
    execute_ppu(ppu);    // 3rd PPU tick
    execute(cpu);        // 1 CPU instruction
    execute_apu(apu);
}
// Render once per frame
render_graphics(g_ctx, ppu->screen);
adjusted_wait(timer);  // Frame timing
```

**Key characteristics:**
- 3 discrete PPU calls per CPU instruction
- Simple state machine in `execute_ppu()` - no rendering
- Frame rendered once at end
- Frame rate throttled

**PPU step function:** `tmp/NES/src/ppu.c:211`
```c
void execute_ppu(PPU* ppu){
    // Simple state machine - just updates dots/scanlines
    // No rendering on every call
    if(ppu->dots == VISIBLE_DOTS + 2 && (ppu->mask & RENDER_ENABLED)){
        // Only set flag, actual rendering happens elsewhere
    }
    // ... advance state ...
}
```

---

### Our Runtime PPU
**File:** `runtime/src/nesrt.c:1058-1077` (interpreter loop)

```c
if (ctx->interpreter_mode) {
    while (!ctx->frame_done) {
        nes_handle_interrupts(ctx);
        ctx->stopped = 0;
        nes_interpret(ctx, ctx->pc);  // ← One instruction
        nes_sync(ctx);                 // ← Heavy PPU sync EVERY instruction
    }
}
```

**nes_sync() function:** `runtime/src/nesrt.c:828-837`
```c
static inline void nes_sync(NESContext* ctx) {
    uint32_t current = ctx->cycles;
    uint32_t delta = current - ctx->last_sync_cycles;
    if (delta > 0) {
        ctx->last_sync_cycles = current;
        if (ctx->ppu) ppu_tick((NESPPU*)ctx->ppu, ctx, delta);  // ← Called EVERY instruction
    }
}
```

**ppu_tick() function:** `runtime/src/ppu.c:747-777`
```c
void ppu_tick(NESPPU* ppu, NESContext* ctx, uint32_t cycles) {
    uint32_t ppu_cycles = cycles * 3;  // ← Multiplies by 3

    // ← LOOP: Runs ppu_cycles times (can be thousands!)
    for (uint32_t i = 0; i < ppu_cycles; i++) {
        ppu_step(ppu, ctx);  // ← May call ppu_render_scanline()
    }
}
```

**ppu_step() function:** `runtime/src/ppu.c:604-640`
```c
static void ppu_step(NESPPU* ppu, NESContext* ctx) {
    ppu->cycle++;
    
    // Render visible scanlines - EXPENSIVE!
    if (ppu->scanline < PPU_VISIBLE_SCANLINES && ppu->cycle == 1) {
        ppu_render_scanline(ppu, ppu->scanline);  // ← Renders entire scanline
    }
    // ... advance state ...
}
```

**Key problems:**
- `ppu_tick()` called after EVERY instruction
- Loops through `cycles * 3` PPU cycles
- Each cycle may trigger full scanline render
- No frame rate throttling

---

### Recompiled Code
**File:** `output/a_v14/a.c` (generated)

Recompiled code uses the same runtime loop but is faster because:
- Executes **blocks of code** between sync points
- Each function (like `func_07_c000()`) executes many instructions
- `nes_tick()` called once per basic block, not per instruction

**Example recompiled function:**
```c
static void func_07_c000(NESContext* ctx) {
    switch (ctx->pc) {
        case 0xc000: goto loc_c000;
        case 0xc003: goto loc_c003;
        // ...
    }
    
loc_c000:
    ctx->f_i = 1;           // SEI
    ctx->x = 0x00;          // LDX #$00
    ctx->pc = 0xc003;
    nes_tick(ctx, 4);       // ← Called once per block, not per instruction
    if (ctx->stopped) return;
    
loc_c003:
    nes_write8(ctx, 0x2000, ctx->x);  // STX $2000
    nes_write8(ctx, 0x2001, ctx->x);  // STX $2001
    // ... many more instructions ...
    nes_tick(ctx, 24);      // ← Batch cycle counting
}
```

**Why recompiled is fast:**
- Executes ~10-50 instructions per `nes_tick()` call
- PPU synced once per block, not per instruction
- Same PPU code, but called less frequently

---

## Why It's Different

| Aspect | Reference | Our Runtime | Recompiled |
|--------|-----------|-------------|------------|
| **Sync Frequency** | Per-instruction (3:1 ratio) | Per-instruction | Per basic block |
| **PPU Step Cost** | Cheap state machine | Expensive render call | Expensive render call |
| **Cycle Processing** | 3 discrete calls | Loop with `cycles * 3` | Loop with `cycles * 3` |
| **Frame Timing** | `adjusted_wait()` throttling | No throttling | No throttling |
| **Instructions/Sync** | 1 | 1 | 10-50 |

**Key insight:** Recompiled code is fast because it batches instructions between sync points. Interpreter syncs after every single instruction.

---

## Fix Implementation

### Option 1: Batch PPU Simulation (Recommended)
**File:** `runtime/src/nesrt.c:1058-1077` (modify interpreter loop)

```c
/* Pure interpreter mode - bypass all recompiled code */
if (ctx->interpreter_mode) {
    /* Batch size: execute this many instructions before PPU sync */
    #define INTERP_BATCH_SIZE 50
    
    while (!ctx->frame_done) {
        nes_handle_interrupts(ctx);

        if (ctx->halted) {
            if (ctx->io[0x0F] & ctx->io[0x80] & 0x1F) {
                ctx->halted = 0;
            }
        }

        /* Batch execute multiple instructions before PPU sync */
        uint32_t batch_start = ctx->cycles;
        for (int i = 0; i < INTERP_BATCH_SIZE && !ctx->frame_done; i++) {
            ctx->stopped = 0;
            nes6502_step(ctx);  /* Just execute, don't sync */
        }
        
        /* Sync PPU once per batch */
        uint32_t batch_cycles = ctx->cycles - batch_start;
        if (batch_cycles > 0 && ctx->ppu) {
            ppu_tick((NESPPU*)ctx->ppu, ctx, batch_cycles);
        }
        
        nes_sync_audio(ctx);  /* Keep audio in sync */
    }
    return ctx->cycles - start;
}
```

**Benefits:**
- Reduces PPU sync calls by 50x
- Matches recompiled code pattern (batch execution)
- Minimal code changes

**Drawbacks:**
- Less accurate CPU-PPU synchronization
- May break games that rely on cycle-exact timing

---

### Option 2: Remove Rendering from ppu_step (Alternative)
**File:** `runtime/src/ppu.c:630-632` (modify ppu_step)

```c
static void ppu_step(NESPPU* ppu, NESContext* ctx) {
    ppu->cycle++;

    if (ppu->cycle >= PPU_CYCLES_PER_SCANLINE) {
        ppu->cycle = 0;
        ppu->scanline++;

        if (ppu->scanline >= PPU_SCANLINES_PER_FRAME) {
            ppu->scanline = 0;
            ppu->frame_number++;
            ctx->frame_done = 1;
        }

        if (ppu->scanline == PPU_VBLANK_SCANLINE) {
            ppu->status |= PPUSTATUS_VBLANK;
            ppu->frame_ready = 1;
            update_vblank_nmi(ppu, ctx);
        } else if (ppu->scanline == PPU_PRE_SCANLINE) {
            ppu->status &= ~(PPUSTATUS_VBLANK | PPUSTATUS_SPRITE_ZERO_HIT);
            ppu->frame_ready = 0;
        }
    }

    /* Render visible scanlines - ONLY at specific cycle */
    /* Changed from: ppu->cycle == 1 */
    /* To: Only render once per scanline at end */
    if (ppu->scanline < PPU_VISIBLE_SCANLINES && 
        ppu->cycle == PPU_CYCLES_PER_SCANLINE - 1) {
        ppu_render_scanline(ppu, ppu->scanline);
    }
}
```

**Benefits:**
- Reduces render calls from "every cycle" to "once per scanline"
- More accurate timing

**Drawbacks:**
- Still calls ppu_step many times per instruction
- Doesn't fix fundamental per-instruction sync issue

---

### Option 3: Hybrid Approach (Best)
Combine both fixes:
1. Batch PPU simulation (reduce sync frequency)
2. Render once per scanline (reduce render frequency)

---

## Testing

### Test Case 1: VBlank Polling Loop
```assembly
; Typical VBlank wait loop
wait_vblank:
    LDA $2002
    BPL wait_vblank
```

**Current:** Takes hours (millions of PPU renders)
**After fix:** Should complete in seconds

### Test Case 2: Parasol Stars
**Expected:** Initialize and show title screen in < 10 seconds
**Current:** Takes hours or never completes
**After fix:** Should initialize in seconds

### Test Case 3: Cycle-Accurate Timing
Some games rely on cycle-exact timing. Test with:
- Demo effects
- Raster interrupts
- Cycle-exact music

**Risk:** Batch execution may break these

---

## Related Files

| Component | Reference | Our Runtime | Recompiled |
|-----------|-----------|-------------|------------|
| Main Loop | `tmp/NES/src/emulator.c:155-176` | `runtime/src/nesrt.c:1058-1077` | N/A |
| PPU Sync | `tmp/NES/src/emulator.c:158-160` | `runtime/src/nesrt.c:828-837` | N/A |
| PPU Step | `tmp/NES/src/ppu.c:211` | `runtime/src/ppu.c:604-640` | N/A |
| CPU Execute | `tmp/NES/src/cpu6502.c:213` | `runtime/src/cpu6502_interp.c:720` | Generated functions |
| Frame Render | `tmp/NES/src/emulator.c:177` | `runtime/src/ppu.c:630-632` | N/A |

---

## Performance Estimates

### Current Interpreter
- Instructions per second: ~1,000
- PPU sync calls per second: ~1,000
- Scanline renders per second: ~10,000 (during visible scanlines)
- Parasol Stars init (256 iterations × 1000 polls): ~4 hours

### After Option 1 (Batching)
- Instructions per second: ~50,000
- PPU sync calls per second: ~1,000 (batched)
- Scanline renders per second: ~10,000
- Parasol Stars init: ~5 minutes

### After Option 2 (Render Once)
- Instructions per second: ~1,000
- PPU sync calls per second: ~1,000
- Scanline renders per second: ~60 (once per scanline per frame)
- Parasol Stars init: ~30 minutes

### After Option 3 (Hybrid)
- Instructions per second: ~50,000
- PPU sync calls per second: ~1,000
- Scanline renders per second: ~60
- Parasol Stars init: ~30 seconds

---

## References

- Investigation: `plan/track5_runtime/phase1_analysis/interpreter_speed_investigation.md`
- Reference emulator: `tmp/NES/src/emulator.c`
- 6502 timing: https://www.nesdev.org/wiki/CPU_timing
