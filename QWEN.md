---
trigger: always_on
---

# Agent Rules for NES Recompiler

## Project Overview
This project is a **static recompiler for NES ROMs**, translating 6502 assembly directly into portable, modern C code. The goal is to run classic NES games without a traditional emulator—just compile and play.

This is a **migration** from the original GameBoy recompiler architecture to support NES hardware.

- **Root Structure**:
  - `recompiler/`: Source for the recompiler tool (`nesrecomp`).
  - `runtime/`: The `nesrt` library linked by recompiled games.
  - `roms/`: Contains original ROM files (e.g., `a.nes`).
  - `output/`: Generated C projects from ROMs.
  - `plan/`: Migration plan and progress tracking.

## Build Standards
- **Build System**: CMake + Ninja. **ALWAYS** use Ninja.
- **Main Project**:
  - Configure: `cmake -G Ninja -B build .`
  - Build: `ninja -C build`

## Workflow: Test ROM Recompilation
To ensure the output reflects the latest recompiler/runtime changes, follow this strictly:

1. **Rebuild Tools**:
   ```bash
   ninja -C build
   ```

2. **Regenerate C Code**:
   Run the recompiler on the test ROM to update source files.
   ```bash
   ./build/bin/nesrecomp roms/a.nes -o output/a
   ```

3. **Rebuild Test Artifact**:
   Rebuild the generated project to produce the final executable.
   ```bash
   cmake -G Ninja -S output/a -B output/a/build
   ninja -C output/a/build
   ```

## Development Guidelines
- **Sync**: "Make sure the recompiled project is always up to date." If you modify `recompiler` logic or `runtime` headers, trigger the *Test ROM Recompilation* workflow.
- **Resources**: Consult `plan/cookie.txt` for current progress and task status.
- **Paths**: Usage of absolute paths is preferred for tool calls, but shell commands should be executed from the workspace root for clarity.
- **Output**: Put all recompiled ROM output in `/output`, since that folder is git ignored.
- **Logs**: Put all `.log` files in `/logs`, since that folder is git ignored.

## Architecture Reference

### CPU (6502)
- **Registers**: A (accumulator), X, Y, SP (stack pointer), PC (program counter)
- **Flags**: N (negative), V (overflow), B (break), D (decimal), I (interrupt), Z (zero), C (carry)
- **Stack**: Fixed at $0100-$01FF (256 bytes)
- **Interrupt Vectors**: NMI ($FFFA), Reset ($FFFC), IRQ ($FFFE)

### Memory Map
| Range | Description |
|-------|-------------|
| $0000-$07FF | 2KB Work RAM |
| $0800-$1FFF | RAM mirrors |
| $2000-$2007 | PPU registers |
| $2008-$3FFF | PPU mirrors |
| $4000-$4017 | APU and I/O |
| $4018-$FFFF | Cartridge space (PRG ROM) |

### PPU (Picture Processing Unit)
- Resolution: 256x240 pixels
- VRAM: 2KB (nametables + pattern tables)
- OAM: 256 bytes (64 sprites × 4 bytes)
- Palette: 32 bytes

### APU (Audio Processing Unit)
- 2 Pulse waves (square)
- 1 Triangle wave
- 1 Noise channel
- 1 DMC (delta modulation channel)

## Debugging
- **Crash/Stuck Analysis**: If the recompiler hangs or produces invalid code:
  - Use `--trace` to log every instruction analyzed: `./build/bin/nesrecomp roms/game.nes --trace`
  - Use `--limit <N>` to stop after N instructions: `./build/bin/nesrecomp roms/game.nes --limit 10000`
  - Watch for `[ERROR]` logs to identify invalid code paths or data misinterpreted as code.

## NES Emulator Reference
The emulator at `/home/sblo/Ohjelmat/NES/` (symlinked to `build/NES/`) serves as the reference implementation:
- **CPU**: `src/cpu6502.c`, `src/cpu6502.h`
- **PPU**: `src/ppu.c`, `src/ppu.h`
- **APU**: `src/apu.c`, `src/apu.h`
- **Mappers**: `src/mappers/`
- **MMU**: `src/mmu.c`, `src/mmu.h`

## Migration Tracks
1. **Track 1 (CPU)**: 6502 decoder, analyzer, IR builder, C emitter
2. **Track 2 (PPU)**: PPU context and rendering
3. **Track 3 (APU)**: APU context and audio
4. **Track 4 (Mappers)**: Mapper interface for banking
5. **Track 5 (Runtime)**: Main NES runtime context
6. **Track 6 (Platform)**: SDL2 platform layer
7. **Track 7 (Integration)**: Build system and first ROM

## Key Differences from GameBoy
| Feature | GameBoy | NES |
|---------|---------|-----|
| CPU | SM83 (Z80-like) | 6502 |
| Clock | ~4.19 MHz | ~1.79 MHz (NTSC) |
| RAM | 8KB WRAM | 2KB Work RAM |
| VRAM | 8KB | 2KB |
| Resolution | 160x144 | 256x240 |
| Banking | MBC (16KB ROM banks) | Mappers (various) |
| Stack | 16-bit (anywhere) | Fixed $0100-$01FF |
