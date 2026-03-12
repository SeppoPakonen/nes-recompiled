# Task 012: First Test ROM Recompilation

## Objective
Get `/tmp/a.nes` (or `roms/a.nes`) recompiled and running.

## Steps

### 1. Prepare Test ROM
```bash
# Copy ROM to project
cp /tmp/a.nes roms/a.nes  # if not already there
```

### 2. Build Recompiler
```bash
cmake -G Ninja -B build .
ninja -C build
```

### 3. Run Recompiler
```bash
./build/bin/nesrecomp roms/a.nes -o output/a
```

### 4. Build Generated Project
```bash
cd output/a
cmake -G Ninja -B build .
ninja -C build
```

### 5. Run Game
```bash
./build/a
```

## Expected Issues
1. **Mapper detection**: If ROM uses unsupported mapper, add minimal support
2. **CHR RAM vs ROM**: Some games use CHR RAM instead of ROM
3. **Mirroring**: Ensure correct nametable mirroring
4. **Timing**: CPU/PPU synchronization may need adjustment

## Debugging Tools
- `--trace`: Log every instruction
- `--limit N`: Stop after N instructions
- `--verbose`: Show analysis stats
- `--disasm`: Disassemble only mode

## Acceptance Criteria
- [ ] ROM loads without errors
- [ ] Analysis completes
- [ ] C code generates
- [ ] Generated code compiles
- [ ] Game runs and displays
- [ ] Input works
- [ ] Audio works (if game has sound)

## Dependencies
All previous tasks

## Estimated Effort
8-16 hours (including debugging)
