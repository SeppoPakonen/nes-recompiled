# Task 007: APU Context Structure

## Objective
Create APU context structure for runtime.

## Background
NES APU has:
- 2 pulse waves (square)
- 1 triangle wave
- 1 noise channel
- 1 DMC (delta modulation channel, optional)
- Memory-mapped registers ($4000-$4017)

## Changes Required

### 1. Create `runtime/include/nesrt_apu.h`
```c
typedef struct {
    // Pulse channels 1 & 2
    struct {
        uint8_t ctrl;
        uint8_t ramp;
        uint16_t freq;
        // ... internal state
    } pulse[2];
    
    // Triangle channel
    struct {
        uint8_t ctrl;
        uint16_t freq;
        uint8_t linear_counter;
    } triangle;
    
    // Noise channel
    struct {
        uint8_t ctrl;
        uint8_t freq;
    } noise;
    
    // DMC (optional)
    struct {
        uint8_t ctrl;
        uint8_t da;
        uint8_t addr;
        uint8_t len;
    } dmc;
    
    // Output
    int16_t sample_left;
    int16_t sample_right;
} NESAPU;
```

### 2. Create `runtime/src/apu.c`
- `nes_apu_init()`
- `nes_apu_read()` - handle $4015
- `nes_apu_write()` - handle $4000-$4017
- `nes_apu_step()` - sample generation

## Acceptance Criteria
- [ ] APU registers readable/writable
- [ ] Basic sound generation
- [ ] Audio callback integration

## Dependencies
None (parallel track)

## Estimated Effort
8-16 hours

## Reference
NES emulator APU at `/home/sblo/Ohjelmat/NES/src/apu.c`
