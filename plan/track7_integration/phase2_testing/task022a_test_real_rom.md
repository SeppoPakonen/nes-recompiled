# Task 022a: Test Real ROM (a.nes / Parasol Stars)

## Objective
Verify the recompiler works with actual NES games.

## Prerequisites
- Task 021b complete (feature tests passing)

## Steps

### 1. Recompile with improved analyzer
```bash
./build/bin/nesrecomp roms/a.nes -o output/a_v3 --verbose 2>&1 | tail -30
```

**Expected improvement:**
- Before fix: 17 functions, 527 IR blocks
- After fix: 50+ functions, 1000+ IR blocks

### 2. Build generated project
```bash
cmake -G Ninja -S output/a_v3 -B output/a_v3/build
ninja -C output/a_v3/build
```

### 3. Run and observe
```bash
timeout 10 ./output/a_v3/build/a 2>&1 | head -50
```

### 4. Check for issues
- [ ] PPU rendering (should see game graphics, not just test pattern)
- [ ] No interpreter fallback for recompiled code
- [ ] Correct branch execution
- [ ] Audio output (if APU code is reached)

### 5. Visual verification
Run the game and check:
- [ ] Does the game display correctly?
- [ ] Are sprites rendered?
- [ ] Is scrolling working?
- [ ] Can you see the title screen?
- [ ] Can you interact with controls?

### 6. Compare analysis stats
```bash
# Compare with previous runs
./build/bin/nesrecomp roms/a.nes -o output/a_v3 --verbose 2>&1 | grep -E "functions|blocks|instructions"
```

## Acceptance Criteria
- [ ] ROM recompiles without errors
- [ ] Generated code builds successfully
- [ ] Game runs and displays title screen
- [ ] Improved analysis coverage vs previous runs
- [ ] Debug output shows meaningful execution
- [ ] Visual output shows game graphics (not just test pattern)

## Dependencies
- Task 021b (feature tests working)

## Estimated Effort
4-8 hours
