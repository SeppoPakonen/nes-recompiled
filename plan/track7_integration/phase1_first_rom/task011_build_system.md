# Task 011: Build System Updates

## Objective
Update CMake build system for NES recompiler.

## Changes Required

### 1. Update root `CMakeLists.txt`
- Rename project from `gb-recompiled` to `nes-recompiled`
- Update output binary name from `gbrecomp` to `nesrecomp`
- Keep SDL2 dependency

### 2. Update `recompiler/CMakeLists.txt`
- Update source files (add 6502 decoder, remove GB-specific)
- Update include paths

### 3. Update `runtime/CMakeLists.txt`
- Add new source files (nesrt.c, ppu.c, apu.c, mapper.c)
- Update library name from `gbrt` to `nesrt`

### 4. Update codegen templates
- Generated CMakeLists for recompiled games
- Generated main.c with NES-specific init

## Acceptance Criteria
- [ ] `cmake -G Ninja -B build .` works
- [ ] `ninja -C build` compiles
- [ ] `nesrecomp` binary produced
- [ ] Generated projects build correctly

## Dependencies
All previous tasks

## Estimated Effort
2-4 hours
