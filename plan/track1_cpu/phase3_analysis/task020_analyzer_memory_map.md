# Task 020: Fix Analyzer for NES Memory Map

## Objective
Fix the analyzer to properly analyze NES PRG ROM at addresses $8000-$FFFF.

## Problem
The analyzer may still skip addresses >= $8000 where NES PRG ROM lives, unlike GameBoy where ROM is at $0000-$7FFF.

## Changes Required

### 1. Check analyzer.cpp for problematic patterns
Search for:
```cpp
if (addr >= 0x8000) continue;  // WRONG for NES!
```

### 2. Fix is_likely_valid_code()
Should accept addresses in $8000-$FFFF range:
```cpp
static bool is_likely_valid_code(const ROM& rom, uint8_t bank, uint16_t addr) {
    // For NES, ROM is at $8000-$FFFF
    if (addr < 0x8000) return false;  // This is RAM/IO for NES
    // ... rest of checks
}
```

### 3. Fix ROM::read_banked()
Ensure correct offset calculation for NES memory map:
- $8000-$BFFF: First 16KB bank
- $C000-$FFFF: Last 16KB bank (or fixed bank for some mappers)

### 4. Test with test ROMs
```bash
./build/bin/nesrecomp testroms/test_cpu.nes -o output/test_cpu --verbose 2>&1 | grep -E "Found|functions|blocks"
```

**Expected:** Should find 10+ functions, not just 2 vectors

## Acceptance Criteria
- [ ] Analyzer finds all functions in test_cpu.nes
- [ ] No ">= 0x8000" skip patterns in analyzer
- [ ] Test ROMs recompile with full code coverage
- [ ] Real ROMs (a.nes) show improved analysis

## Dependencies
None - foundational fix

## Estimated Effort
2-4 hours
