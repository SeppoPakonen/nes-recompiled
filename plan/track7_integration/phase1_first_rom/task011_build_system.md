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
- [x] `cmake -G Ninja -B build .` works
- [x] `ninja -C build` compiles
- [x] `nesrecomp` binary produced
- [x] Generated projects build correctly

## Dependencies
All previous tasks

## Estimated Effort
2-4 hours

## Status
**COMPLETED** (2026-03-12)

### Changes Made
- Updated root CMakeLists.txt: project name to "nes-recompiled"
- Updated recompiler/CMakeLists.txt: binary name to "nesrecomp"
- Updated c_emitter.cpp: added mapper.c to generated CMakeLists
- All acceptance criteria met
