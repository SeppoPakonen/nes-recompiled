# Task 024: Expand Test Coverage

## Objective
Create additional specialized test ROMs for features not yet covered.

## New Test ROMs to Create

### 1. test_sprites.asm
**Features:**
- OAM DMA transfer ($4014)
- Sprite priority (background vs sprite)
- Sprite 0 hit flag
- 8x8 vs 8x16 sprite modes
- Sprite X/Y coordinates
- Tile index and palette selection

**Expected output:**
```
SPRITE TEST
OAM DMA OK
PRIO OK
HIT OK
SIZE OK
DONE
```

### 2. test_scroll.asm
**Features:**
- Fine scrolling (bits 0-2 of $2005)
- Coarse scrolling (bits 3-7 of $2005)
- Nametable switching ($2000 bits 0-1)
- Mid-frame scroll changes
- Horizontal vs vertical scroll

**Expected output:**
```
SCROLL TEST
FINE OK
COARSE OK
NAME OK
MIDFRAME OK
DONE
```

### 3. test_irq.asm
**Features:**
- IRQ generation from frame counter
- IRQ with scanline timing
- IRQ vs NMI priority
- IRQ disable/enable ($4017, CLI, SEI)
- IRQ handler execution

**Expected output:**
```
IRQ TEST
GEN OK
TIME OK
PRIO OK
HANDLER OK
DONE
```

### 4. test_bank.asm (for MMC1)
**Features:**
- MMC1 shift register writes
- PRG bank switching
- CHR bank switching
- Mirroring changes
- Bank latch reset

**Expected output:**
```
BANK TEST
SHIFT OK
PRG OK
CHR OK
MIRROR OK
DONE
```

## Implementation

For each test:
1. Create assembly source in `testroms/src/features/`
2. Create Python builder `testroms/build_<name>.py`
3. Add to `testroms/build_features.sh`
4. Document in `FEATURE_COVERAGE.md`
5. Add debug output at $6000

## Acceptance Criteria
- [ ] 4 new test ROMs created
- [ ] All build successfully
- [ ] All produce expected debug output
- [ ] All recompile and run correctly
- [ ] Documentation updated

## Dependencies
- Task 021 (end-to-end testing working)

## Estimated Effort
8-16 hours (2-4 hours per test)
