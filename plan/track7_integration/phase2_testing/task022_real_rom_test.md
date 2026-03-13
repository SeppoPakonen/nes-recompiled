# Task 022: Test with Real ROM (a.nes / Parasol Stars)

## Objective
Verify the recompiler works with actual NES games beyond test ROMs.

## Prerequisites
- Task 020 complete (analyzer fixed)
- Task 021 complete (test ROMs working)

## Steps

### 1. Recompile a.nes with latest fixes
```bash
./build/bin/nesrecomp roms/a.nes -o output/a_v2 --verbose 2>&1 | tail -20
```

### 2. Build generated project
```bash
cmake -G Ninja -S output/a_v2 -B output/a_v2/build
ninja -C output/a_v2/build
```

### 3. Run and observe
```bash
timeout 10 ./output/a_v2/build/a 2>&1 | head -50
```

### 4. Check for issues
- PPU rendering (should see game graphics, not just test pattern)
- No interpreter fallback for recompiled code
- Correct branch execution
- Audio output (if APU code is reached)

### 5. Visual verification
- Does the game display correctly?
- Are sprites rendered?
- Is scrolling working?
- Can you see the title screen?

### 6. Compare with previous runs
```bash
# Compare analysis stats
./build/bin/nesrecomp roms/a.nes -o output/a_v2 --verbose 2>&1 | grep -E "functions|blocks|instructions"
```

**Expected improvement:**
- Before: 17 functions, 527 IR blocks
- After: 50+ functions, 1000+ IR blocks (depending on analyzer fix)

## Acceptance Criteria
- [ ] ROM recompiles without errors
- [ ] Generated code builds successfully
- [ ] Game runs and displays title screen
- [ ] Improved analysis coverage vs previous runs
- [ ] Debug output shows meaningful execution

## Dependencies
- Task 020 (analyzer fix)
- Task 021 (test ROMs working)

## Estimated Effort
4-8 hours
