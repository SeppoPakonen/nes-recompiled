# Task 010: SDL Platform Layer

## Objective
Create SDL2 platform layer for NES runtime.

## Changes Required

### 1. Create `runtime/include/nesrt_platform_sdl.h`

### 2. Create `runtime/src/platform_sdl.c`
- SDL2 window creation (256x240, scaled)
- Input handling (keyboard -> NES controller)
- Audio output (SDL audio callback)
- Main loop integration

### 3. NES Controller Mapping
```
NES Button    Keyboard
A             Z / J
B             X / K
Select        Right Shift / Backspace
Start         Enter / -
D-Pad Up      Up Arrow / W
D-Pad Down    Down Arrow / S
D-Pad Left    Left Arrow / A
D-Pad Right   Right Arrow / D
```

### 4. Main Loop
```c
while (!ctx->exit) {
    handle_events();
    nes_run_frame(ctx);
    render_framebuffer(ctx->ppu->framebuffer);
    sync_audio();
}
```

## Acceptance Criteria
- [ ] Window opens
- [ ] Keyboard input works
- [ ] Audio plays
- [ ] Frame timing correct (60fps NTSC)

## Dependencies
- Task 009 (NES Runtime)

## Estimated Effort
8-12 hours

## Reference
GB runtime `runtime/src/platform_sdl.cpp`
