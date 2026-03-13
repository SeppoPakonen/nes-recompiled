# Task 021: End-to-End Test with Debug Output

## Objective
Run feature test ROMs and verify debug output matches expected behavior.

## Prerequisites
- Task 020 complete (analyzer fixed)
- Debug port logging implemented (done)

## Steps

### 1. Recompile test_cpu.nes
```bash
./build/bin/nesrecomp testroms/test_cpu.nes -o output/test_cpu
cmake -G Ninja -S output/test_cpu -B output/test_cpu/build
ninja -C output/test_cpu/build
```

### 2. Run and capture output
```bash
timeout 5 ./output/test_cpu/build/test_cpu 2>&1 | tee test_cpu_output.txt
```

### 3. Verify expected output
Expected:
```
CPU TEST
LD OK
ARI OK
LOG OK
CMP OK
BR OK
JSR OK
RTS OK
DONE
```

### 4. Check generated C code
```bash
# Verify branch conditions
grep -A3 "BEQ\|BNE" output/test_cpu/test_cpu.c

# Verify RTS
grep -A2 "RTS" output/test_cpu/test_cpu.c

# Verify INC/DEC memory
grep -A5 "INC" output/test_cpu/test_cpu.c
```

### 5. Repeat for other feature tests
- test_ppu.nes - PPU register writes
- test_apu.nes - APU channel setup
- test_input.nes - Controller reading
- test_timer.nes - Timing tests

## Acceptance Criteria
- [ ] test_cpu.nes outputs all test results
- [ ] Generated C code matches assembly intent
- [ ] All 5 feature tests run without crashes
- [ ] Debug output matches FEATURE_COVERAGE.md expectations

## Dependencies
- Task 020 (analyzer fix)
- Debug port logging (done)

## Estimated Effort
4-8 hours
