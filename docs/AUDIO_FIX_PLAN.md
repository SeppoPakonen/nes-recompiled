# Audio Performance Fix Plan for NES Recompiler

## Problem Summary

Audio issues may occur when running NES games through the recompiler. This document tracks known audio problems and their fixes for the NES APU.

## NES APU Architecture

The NES Audio Processing Unit has 5 channels:

1. **Pulse Channel 1** ($4000-$4003): Square wave with duty cycle
2. **Pulse Channel 2** ($4004-$4007): Second square wave
3. **Triangle Channel** ($4008-$400B): Triangle wave (no volume control)
4. **Noise Channel** ($400C-$400F): Pseudo-random noise
5. **DMC** ($4010-$4013): Delta modulation channel (sample playback)

## Common Audio Issues

### 1. Sample Rate Drift

**Cause**: Integer division in sample timing
```c
// Wrong: integer division
apu->sample_period = 95;  // Should be 95.108...

// Correct: fixed-point or accumulator
apu->sample_accum += cycles;
if (apu->sample_accum >= SAMPLE_PERIOD) {
    apu->sample_accum -= SAMPLE_PERIOD;
    // Generate sample
}
```

**Fix**: Use fractional accumulator for precise timing

### 2. DMC DMA Conflicts

**Cause**: DMC channel steals CPU cycles for DMA
- DMC reads from memory every ~128 cycles
- Can conflict with PPU DMA or CPU access

**Fix**: Account for DMC cycle stealing in timing

### 3. Noise Channel LFSR Issues

**Cause**: LFSR can spin excessively at high frequencies
```c
// Problematic: loop can iterate thousands of times
while (apu->noise.accum >= period) {
    apu->noise.accum -= period;
    // LFSR shift...
}

// Better: pre-calculate iterations
int iterations = apu->noise.accum / period;
apu->noise.accum -= iterations * period;
// Advance LFSR by 'iterations' steps
```

**Fix**: Pre-calculate iteration count

### 4. Frame IRQ Timing

**Cause**: APU frame counter runs at ~240Hz
- Mode 0: IRQ enabled, fires every 7457 cycles
- Mode 1: IRQ disabled, different timing

**Fix**: Accurate frame counter emulation

## Implementation Plan

### Phase 1: Basic Audio

1. **Implement all 5 channels**
   - Pulse 1 & 2 with length counter and envelope
   - Triangle with linear counter
   - Noise with LFSR
   - DMC (optional, complex)

2. **Sample output**
   - Mix to 44.1kHz or 48kHz
   - SDL2 audio callback

### Phase 2: Accuracy

1. **Precise timing**
   - Fixed-point sample timing
   - Accurate frame IRQ

2. **Optimization**
   - LFSR lookup tables
   - Batch sample generation

### Phase 3: Debugging

1. **Audio stats overlay**
   - Queue size
   - Samples generated/dropped
   - Per-channel volume

2. **WAV export**
   - Debug audio capture
   - Compare with reference emulator

## Files to Modify

| File | Purpose |
|------|---------|
| `runtime/include/nesrt_apu.h` | APU context structure |
| `runtime/src/apu.c` | APU emulation |
| `runtime/src/platform_sdl.c` | Audio output |

## Testing

```bash
# Build with audio
ninja -C build

# Run game
./output/game/build/game

# Export audio for analysis
./output/game/build/game --dump-audio debug.wav
```

## Reference

- [NesDev APU Documentation](https://www.nesdev.org/wiki/APU)
- [NSF Format Specification](https://www.nesdev.org/wiki/NSF)
- Reference emulator: `/home/sblo/Ohjelmat/NES/src/apu.c`
