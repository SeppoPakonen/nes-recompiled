# Task 021a: Fix Test ROM Builder JSR Address Calculation

## Objective
Fix `build_test_cpu.py` to generate correct JSR target addresses.

## Problem
The subroutine address calculation is wrong:
```python
subroutine_addr = 0x8000 + len(prg) + 7 + 3  # WRONG!
```

This causes JSR instructions to point to invalid addresses (header/zero page area instead of ROM code).

## Changes Required

### 1. Fix subroutine_addr calculation
**File:** `testroms/build_test_cpu.py`

**Current (WRONG):**
```python
subroutine_addr = 0x8000 + len(prg) + 7 + 3
```

**Should be:**
```python
# JSR is 3 bytes: opcode (1) + target low (1) + target high (1)
# After JSR, PC points to next instruction
# RTS returns to address after JSR
subroutine_addr = 0x8000 + len(prg) + 3  # Point to code after JSR instruction
```

### 2. Fix all similar calculations
Search for patterns like:
- `0x8000 + len(prg) + X` where X is arbitrary
- `loop_addr = 0x8000 + len(prg) + Y`

Ensure all address calculations account for:
- Current PRG length
- Instruction size (JSR = 3 bytes, JMP = 3 bytes, etc.)
- No arbitrary offsets

### 3. Verify all JSR/JMP targets
After fixing, run verification:
```bash
python3 -c "
with open('testroms/test_cpu.nes', 'rb') as f:
    rom = f.read()[16:]  # Skip header
for i, b in enumerate(rom):
    if b == 0x20:  # JSR
        target = rom[i+1] | (rom[i+2] << 8)
        if target >= 0x8000 and target < 0xC000:
            print(f'✓ JSR at 0x{i:04x} -> 0x{target:04x} (valid)')
        else:
            print(f'✗ JSR at 0x{i:04x} -> 0x{target:04x} (INVALID)')
"
```

**Expected:** All JSR targets should be in 0x8000-0xBFFF range

## Acceptance Criteria
- [ ] All JSR targets point to valid ROM addresses (0x8000-0xBFFF)
- [ ] Recompiler finds 10+ functions (was 3)
- [ ] Recompiler finds 50+ basic blocks (was 24)
- [ ] Test ROM runs and outputs "CPU TEST... DONE"
- [ ] No infinite loops in execution

## Dependencies
None - this is a test ROM fix

## Estimated Effort
1-2 hours
