# Task 019: Update AUDIO_FIX_PLAN.md for NES

## Objective
Adapt audio fix plan for NES APU instead of GameBoy APU.

## Changes Required

### APU Architecture
- Change from GB APU (4 channels) to NES APU (5 channels)
  - 2 Pulse waves
  - 1 Triangle
  - 1 Noise
  - 1 DMC (delta modulation)

### Technical Details
- Update sample rate references (NES: ~44.1kHz output)
- Update cycle counts (NES CPU: 1.79MHz vs GB: 4.19MHz)
- Update register addresses ($4000-$4017 vs GB IO)

### File Locations
- Change `runtime/src/audio.c` → `runtime/src/apu.c`
- Change `runtime/src/platform_sdl.cpp` (keep same)
- Update include paths

### Channel-Specific Issues
- Update noise channel LFSR differences
- Add DMC DMA considerations (if implemented)

## Acceptance Criteria
- [ ] APU architecture accurate for NES
- [ ] Fix plan applicable
- [ ] File paths correct

## Dependencies
Task 007 (APU context)

## Estimated Effort
2-4 hours
