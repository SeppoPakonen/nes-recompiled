# NES Recompiled

A **static recompiler** for original NES ROMs that translates 6502 assembly directly into portable, modern C code. Run your favorite classic games without a traditional emulator—just compile and play.

![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20Windows-lightgrey)

<p align="center">
  <img src="dino.png" alt="NES Recompiled Screenshot" width="512">
</p>

---

## Downloads

Pre-built binaries are available on the [Releases](https://github.com/arcanite24/nes-recompiled/releases) page:

| Platform | Architecture | File |
|----------|--------------|------|
| **Windows** | x64 | `nes-recompiled-windows-x64.zip` |
| **Linux** | x64 | `nes-recompiled-linux-x64.tar.gz` |
| **macOS** | x64 (Intel) | `nes-recompiled-macos-x64.tar.gz` |
| **macOS** | ARM64 (Apple Silicon) | `nes-recompiled-macos-arm64.tar.gz` |

> **Note**: The recompiler (`nesrecomp`) is what you download. After recompiling a ROM, you'll still need CMake, Ninja, SDL2, and a C compiler to build the generated project.

---

## Features

- **Native Performance**: Generated C code compiles to native machine code
- **Accurate Runtime**:
  - Cycle-accurate 6502 instruction emulation
  - Precise PPU (graphics) emulation with scanline rendering
  - Audio subsystem (APU) with all 5 channels (2 pulse, triangle, noise, DMC)
- **Mapper Support**: MMC1 (SMB, Zelda), MMC3 (SMB3, Megaman), and more
- **SDL2 Platform Layer**: Ready-to-run with keyboard/controller input and window display
- **Debugging Tools**: Trace logging, instruction limits, and screenshot capture
- **Cross-Platform**: Works on macOS, Linux, and Windows (via CMake + Ninja)

---

## Quick Start

### Prerequisites

- **CMake** 3.15+
- **Ninja** build system
- **SDL2** development libraries
- A C/C++ compiler (Clang, GCC, or MSVC)

### Building

```bash
# Clone and enter the repository
git clone https://github.com/arcanite24/nes-recompiled.git
cd nes-recompiled

# Configure and build
cmake -G Ninja -B build .
ninja -C build
```

### Recompiling a ROM

```bash
# Generate C code from a ROM
./build/bin/nesrecomp path/to/game.nes -o output/game

# Build the generated project
cmake -G Ninja -S output/game -B output/game/build
ninja -C output/game/build

# Run!
./output/game/build/game
```

---

## Quick Setup

### Automated Setup (Recommended)

**macOS/Linux:**
```bash
# Download and run the setup script
git clone https://github.com/arcanite24/nes-recompiled.git
cd nes-recompiled
chmod +x tools/setup.sh
./tools/setup.sh
```

**Windows:**
```bash
# Download and run the setup script (run as Administrator)
git clone https://github.com/arcanite24/nes-recompiled.git
cd nes-recompiled
powershell -ExecutionPolicy Bypass -File tools/setup.ps1
```

### Manual Setup

**Prerequisites:**
- CMake 3.15+
- Ninja build system
- SDL2 development libraries
- A C/C++ compiler (Clang, GCC, or MSVC)

**Building:**
```bash
# Clone and enter the repository
git clone https://github.com/arcanite24/nes-recompiled.git
cd nes-recompiled

# Configure and build
cmake -G Ninja -B build .
ninja -C build
```

## Usage

### Basic Recompilation

```bash
./build/bin/nesrecomp <rom.nes> -o <output_dir>
```

The recompiler will:

1. Load and parse the iNES ROM header
2. Analyze control flow starting from reset vector ($FFFC)
3. Decode 6502 instructions and track mapper state
4. Generate C source files with the runtime library

### Debugging Options

| Flag | Description |
|------|-------------|
| `--trace` | Print every instruction during analysis |
| `--limit <N>` | Stop analysis after N instructions |
| `--add-entry-point <addr>` | Manually specified entry point (e.g. `C000`) |
| `--no-scan` | Disable aggressive code scanning (enabled by default) |
| `--verbose` | Show detailed analysis statistics |
| `--use-trace <file>` | Use runtime trace to seed entry points |

**Example:**

```bash
# Debug a problematic ROM
./build/bin/nesrecomp game.nes -o output/game --trace --limit 5000
```

### Advanced Usage

**Trace-Guided Recompilation (Recommended):**
Complex games often use computed jumps that static analysis cannot resolve. You can use execution traces to "seed" the analyzer with every instruction physically executed during a real emulated session.

1. **Generate a trace**: Run any recompiled version of the game with tracing enabled.

   ```bash
   # Using recompiled game
   ./output/game/build/game --trace-entries game.trace --limit 1000000
   ```

2. **Recompile with grounding**: Feed the trace back into the recompiler.

   ```bash
   ./build/bin/nesrecomp roms/game.nes -o output/game --use-trace game.trace
   ```

For a detailed walkthrough, see **[GROUND_TRUTH_WORKFLOW.md](GROUND_TRUTH_WORKFLOW.md)**.

**Manual Entry Points:**
If you see `[NES] Interpreter` messages in the logs at specific addresses, you can manually force the recompiler to analyze them:

```bash
./build/bin/nesrecomp roms/game.nes -o out_dir --add-entry-point C000
```

**Aggressive Scanning:**
The recompiler automatically scans memory for code that isn't directly reachable (e.g. unreferenced functions). This improves compatibility but may occasionally misidentify data as code. To disable it:

```bash
./build/bin/nesrecomp roms/game.nes -o out_dir --no-scan
```

### Runtime Options

When running a recompiled game:

| Option | Description |
|--------|-------------|
| `--input <script>` | Automate input from a script file |
| `--dump-frames <list>` | Dump specific frames as screenshots |
| `--screenshot-prefix <path>` | Set screenshot output path |
| `--trace-entries <file>` | Log all executed (PC) points to file |

### Controls

| NES | Keyboard (Primary) | Keyboard (Alt) |
|-----|-------------------|----------------|
| **D-Pad Up** | ↑ Arrow | W |
| **D-Pad Down** | ↓ Arrow | S |
| **D-Pad Left** | ← Arrow | A |
| **D-Pad Right** | → Arrow | D |
| **A Button** | Z | J |
| **B Button** | X | K |
| **Start** | Enter | - |
| **Select** | Right Shift | Backspace |
| **Quit** | Escape | - |

---

## How It Works

### 1. Analysis Phase

The recompiler performs static control flow analysis:

- Discovers all reachable code starting from reset vector ($FFFC)
- Follows JSR calls and JMP jumps
- Detects computed jumps (e.g., `JMP (addr)`) and resolves jump tables
- Separates code from data using heuristics

### 2. Code Generation

Instructions are translated to C:

```c
// Original: LDA #$42
ctx->a = 0x42;
nes_set_flags_zn(ctx, ctx->a);

// Original: ADC #$01
ctx->a = nes_adc(ctx, ctx->a, 0x01);

// Original: JSR $C000
nes_call(ctx, 0xC000);
return;
```

### 3. Runtime Execution

The generated code links against `libnesrt`, which provides:

- Memory-mapped I/O (`nes_read8`, `nes_write8`)
- CPU flag manipulation (N, V, Z, C, etc.)
- PPU scanline rendering (256x240)
- Audio sample generation (5 channels)
- Timer and interrupt handling (NMI, IRQ)

---

## Compatibility

See [COMPATIBILITY.md](COMPATIBILITY.md) for the full test report.

| Status | Count | Percentage |
|--------|-------|------------|
| ✅ SUCCESS | TBD | TBD |
| ❌ RECOMPILE_FAIL | TBD | TBD |
| ⚠️ RUN_TIMEOUT | TBD | TBD |
| 🔧 EXCEPTION | TBD | TBD |

Manually confirmed working examples:

- **TBD** - Testing in progress

---

## Roadmap

- [x] Migration plan from GameBoy recompiler
- [ ] 6502 CPU decoder and analyzer
- [ ] PPU (graphics) emulation
- [ ] APU (audio) emulation
- [ ] Mapper support (MMC1, MMC3, etc.)
- [ ] First playable ROM
- [ ] Tools for better graphical debugging
- [ ] Android builds
- [ ] Famicom Disk System support
- [ ] Improve quality of generated code
- [ ] Reduce size of output binaries

---

## Tools

The `tools/` directory contains utilities for analysis and verification:

### 1. Ground Truth Capturer

Automate instruction discovery using a headless emulator.

```bash
python3 tools/capture_ground_truth.py roms/game.nes --frames 3600 --random -o game.trace
```

### 2. Coverage Analyzer

Audit your recompiled code against a dynamic trace to see exactly what instructions are missing.

```bash
python3 tools/compare_ground_truth.py --trace game.trace output/game
```

---

## Development

### Project Architecture

The recompiler uses a multi-stage pipeline:

```
ROM → Decoder → IR Builder → Analyzer → C Emitter → Output
         ↓           ↓            ↓
     Opcodes   Intermediate   Control Flow
               Representation   Graph
```

Key components:

- **Decoder** (`decoder_6502.h`): Parses raw bytes into 6502 opcodes
- **IR Builder** (`ir_builder.h`): Converts opcodes to intermediate representation
- **Analyzer** (`analyzer.h`): Builds control flow graph from reset vector
- **C Emitter** (`c_emitter.h`): Generates C code from IR

---

## License

This project is licensed under the MIT License.

**Note**: NES is a trademark of Nintendo. This project does not include any copyrighted ROM data. You must provide your own legally obtained ROM files.

---

## Acknowledgments

- [NesDev Wiki](https://www.nesdev.org/wiki/) - The definitive NES technical reference
- [6502.org](http://www.6502.org/) - 6502 CPU documentation
- The NesDev community for extensive documentation and test ROMs
- [N64Recomp](https://github.com/Mr-Wiseguy/N64Recomp) - The original recompiler that inspired this project
- [gb-recompiled](https://github.com/arcanite24/gb-recompiled) - The GameBoy recompiler this project was migrated from
