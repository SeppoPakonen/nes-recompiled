# Task 021b: Run End-to-End Feature Tests

## Objective
Run all 5 feature test ROMs and verify debug output.

## Prerequisites
- Task 021a complete (test ROM builder fixed)

## Steps

### 1. Rebuild all feature test ROMs
```bash
cd testroms
./build_features.sh
```

### 2. Recompile each test ROM
```bash
# CPU test
./build/bin/nesrecomp testroms/test_cpu.nes -o output/test_cpu_v2 --verbose 2>&1 | grep -E "Found|functions|blocks"

# PPU test
./build/bin/nesrecomp testroms/test_ppu.nes -o output/test_ppu_v2 --verbose 2>&1 | grep -E "Found|functions|blocks"

# APU test
./build/bin/nesrecomp testroms/test_apu.nes -o output/test_apu_v2 --verbose 2>&1 | grep -E "Found|functions|blocks"

# Input test
./build/bin/nesrecomp testroms/test_input.nes -o output/test_input_v2 --verbose 2>&1 | grep -E "Found|functions|blocks"

# Timer test
./build/bin/nesrecomp testroms/test_timer.nes -o output/test_timer_v2 --verbose 2>&1 | grep -E "Found|functions|blocks"
```

**Expected:** Each should find 10+ functions, 30+ basic blocks

### 3. Build and run each test
```bash
for test in cpu ppu apu input timer; do
    echo "=== Testing test_$test ==="
    cmake -G Ninja -S output/test_$test_v2 -B output/test_$test_v2/build
    ninja -C output/test_$test_v2/build
    timeout 5 ./output/test_$test_v2/build/test_$test 2>&1 | head -20
    echo ""
done
```

### 4. Verify expected output

| Test ROM | Expected Output |
|----------|----------------|
| test_cpu | "CPU TEST" ... "LD OK" ... "DONE" |
| test_ppu | "PPU TEST" ... "VRAM OK" ... "DONE" |
| test_apu | "APU TEST" ... "PULSE OK" ... "DONE" |
| test_input | "INPUT TEST" ... "JOY OK" ... "DONE" |
| test_timer | "TIMER TEST" ... "CYCLE OK" ... "DONE" |

### 5. Document any failures
For each test that doesn't produce expected output:
- Capture full output
- Check generated C code for issues
- Compare with assembly source
- File bug report

## Acceptance Criteria
- [ ] All 5 test ROMs recompile successfully
- [ ] All 5 test ROMs build without errors
- [ ] All 5 test ROMs run and produce expected output
- [ ] Debug output shows "OK" for all test sections
- [ ] No infinite loops or crashes

## Dependencies
- Task 021a (test ROM builder fixed)

## Estimated Effort
2-4 hours
