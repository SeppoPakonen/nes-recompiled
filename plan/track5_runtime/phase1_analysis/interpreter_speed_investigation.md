# Task: Interpreter Speed Investigation

## Status
✅ COMPLETED

## Objective
Investigate why the reference emulator runs at full speed while our interpreter mode is extremely slow (games take hours to initialize).

## Findings

### Root Cause

The interpreter is slow because of **per-instruction PPU synchronization with expensive rendering**:

1. **Every 6502 instruction** calls `nes_sync()` which calls `ppu_tick()`
2. **`ppu_tick()` loops through `cycles * 3` PPU cycles**
3. **Each PPU cycle may trigger `ppu_render_scanline()`** which renders an entire scanline
4. **Games poll PPUSTATUS in tight loops** (hundreds of thousands of iterations)
5. **Result:** Millions of scanline renders for a simple initialization loop

### Call Chain

```
nes_run_frame() [nesrt.c:1058]
  └─> while (!ctx->frame_done)
       ├─> nes_interpret(ctx, ctx->pc) [interpreter.c:16]
       │    └─> nes6502_step(ctx) [cpu6502_interp.c:720]
       │         └─> nes_add_cycles(ctx, cycles) [nesrt.c:841]
       │              └─> ctx->cycles += cycles
       └─> nes_sync(ctx) [nesrt.c:828]
            └─> ppu_tick(ppu, ctx, delta) [ppu.c:747]
                 └─> for (i = 0; i < ppu_cycles; i++)  // ppu_cycles = delta * 3
                      └─> ppu_step(ppu, ctx) [ppu.c:604]
                           └─> ppu_render_scanline() [EXPENSIVE!]
```

### Example Calculation

Game polls `$2002` 100,000 times waiting for VBlank:
- Each poll: `LDA $2002` (4 cycles) + `BPL` (2 cycles) = 6 cycles
- Total CPU cycles: 600,000
- Total PPU cycles simulated: 1,800,000
- **Our interpreter:** Potentially renders scanline on every PPU cycle during visible scanlines
- **Reference:** Only renders once per scanline at the correct time

---

## Comparison: Reference vs Our Runtime

### Reference Emulator Main Loop
**File:** `tmp/NES/src/emulator.c:155-176`

```c
while (!ppu->render) {
    execute_ppu(ppu);    // 1st PPU tick - simple state machine
    execute_ppu(ppu);    // 2nd PPU tick
    execute_ppu(ppu);    // 3rd PPU tick
    execute(cpu);        // 1 CPU instruction
    execute_apu(apu);    // APU tick
}
render_graphics(g_ctx, ppu->screen);  // Render once per frame
adjusted_wait(timer);  // Frame timing control
```

**Key characteristics:**
- 3 discrete PPU calls per CPU instruction (correct 3:1 ratio)
- Simple state machine in `execute_ppu()` - no rendering
- Frame rendered once at end of frame
- Frame rate throttled with `adjusted_wait()`

---

### Our Runtime Interpreter Loop
**File:** `runtime/src/nesrt.c:1058-1077`

```c
if (ctx->interpreter_mode) {
    while (!ctx->frame_done) {
        nes_handle_interrupts(ctx);
        ctx->stopped = 0;
        nes_interpret(ctx, ctx->pc);  // Pure interpretation
        nes_sync(ctx);                 // ← PROBLEM: Called EVERY instruction
    }
}
```

**nes_sync() function:**
**File:** `runtime/src/nesrt.c:828-837`

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

**ppu_tick() function:**
**File:** `runtime/src/ppu.c:747-777`

```c
void ppu_tick(NESPPU* ppu, NESContext* ctx, uint32_t cycles) {
    uint32_t ppu_cycles = cycles * 3;  // ← Multiplies by 3

    // Fast-forward workaround (only helps first VBlank)
    if (!fast_forward_done) {
        ppu->scanline = PPU_VBLANK_SCANLINE;
        vblank_hold_ticks = 10;
        fast_forward_done = true;
    }

    // ← LOOP: Runs ppu_cycles times (can be thousands!)
    for (uint32_t i = 0; i < ppu_cycles; i++) {
        ppu_step(ppu, ctx);  // ← May call ppu_render_scanline()
    }
}
```

**Key problems:**
- `ppu_tick()` called after EVERY instruction
- Loops through `cycles * 3` PPU cycles
- Each cycle may trigger full scanline render
- No frame rate throttling

---

## Three-Way Fix Locations

| Component | Reference Emulator | Our Runtime | Recompiled Code |
|-----------|-------------------|-------------|-----------------|
| **Main Loop** | `tmp/NES/src/emulator.c:155-176` | `runtime/src/nesrt.c:1058-1077` | N/A (uses runtime loop) |
| **PPU Sync** | `tmp/NES/src/emulator.c:158-160` (3 discrete calls) | `runtime/src/nesrt.c:828-837` (per-instruction) | N/A |
| **PPU Step** | `tmp/NES/src/ppu.c:211` (simple state machine) | `runtime/src/ppu.c:604-640` (calls render) | N/A |
| **CPU Execute** | `tmp/NES/src/cpu6502.c:213` | `runtime/src/cpu6502_interp.c:720` | Generated C functions |
| **Frame Render** | `tmp/NES/src/emulator.c:177` (once per frame) | `runtime/src/ppu.c:630-632` (potentially every cycle) | N/A |
| **Frame Timing** | `tmp/NES/src/timers.c:adjusted_wait()` | None | N/A |

---

## Why It's Different in All Three Targets

### Reference Emulator
- **Architecture:** Tight CPU-PPU synchronization loop
- **PPU Step:** Simple state machine, no rendering
- **Rendering:** Once per frame at end
- **Timing:** Frame rate throttled

### Our Runtime PPU
- **Architecture:** Per-instruction PPU sync
- **PPU Step:** Calls `ppu_render_scanline()` during visible scanlines
- **Rendering:** Potentially every PPU cycle
- **Timing:** No throttling, simulates every cycle

### Recompiled Code
- **Architecture:** Uses runtime's `nes_run_frame()` loop
- **PPU Sync:** Same as runtime (calls `nes_sync()` after each block)
- **Rendering:** Same as runtime
- **Timing:** Same as runtime

**Key insight:** Recompiled code is fast because it executes **blocks of code** between sync points, while interpreter executes **one instruction** between sync points.

---

## Recommended Fixes (Priority Order)

### 1. Batch PPU Simulation (HIGH Priority)
**File:** `runtime/src/nesrt.c:1058-1077`

Instead of syncing after every instruction, batch multiple instructions:

```c
if (ctx->interpreter_mode) {
    while (!ctx->frame_done) {
        nes_handle_interrupts(ctx);
        
        // Batch execute multiple instructions before PPU sync
        uint32_t batch_start = ctx->cycles;
        for (int i = 0; i < 100 && !ctx->frame_done; i++) {
            ctx->stopped = 0;
            nes6502_step(ctx);  // Just execute, don't sync
        }
        
        // Sync PPU once per batch
        uint32_t batch_cycles = ctx->cycles - batch_start;
        if (batch_cycles > 0 && ctx->ppu) {
            ppu_tick((NESPPU*)ctx->ppu, ctx, batch_cycles);
        }
    }
}
```

### 2. Remove Scanline Rendering from ppu_step (MEDIUM Priority)
**File:** `runtime/src/ppu.c:630-632`

Only render at specific PPU cycle, not every cycle during visible scanline:

```c
// Only render at specific cycle, not every cycle
if (ppu->scanline < PPU_VISIBLE_SCANLINES && ppu->cycle == PPU_RENDER_START_CYCLE) {
    ppu_render_scanline(ppu, ppu->scanline);
}
```

### 3. Add Frame Rate Throttling (LOW Priority)
**File:** `runtime/src/nesrt.c:1058-1077`

Add frame timing similar to reference emulator's `adjusted_wait()`.

---

## Related Tasks

- See `plan/track2_ppu/phase1_analysis/ppu_comparison.md` for detailed PPU comparison
- See `plan/track1_cpu/phase1_analysis/interpreter_design.md` for interpreter architecture

---

## References

- Reference emulator: `tmp/NES/src/`
- Our runtime: `runtime/src/`
- Generated code: `output/a_v14/a.c`
