# NES Recompiler Compatibility Report

**Total ROMs Processed**: TBD
**Success Rate**: TBD

> **Note**: This is a new project migrated from GB Recompiled. Compatibility testing is in progress.

## Testing Status

| ROM File | Title | Mapper | Status | Notes |
| --- | --- | --- | --- | --- |
| a.nes | Test ROM | TBD | TBD | Primary test ROM |
| Super Mario Bros. (USA).nes | Super Mario Bros. | 0 | Pending | NROM - basic mapper |
| The Legend of Zelda (USA).nes | Zelda | 1 | Pending | MMC1 |
| Metroid (USA).nes | Metroid | 1 | Pending | MMC1 |
| Mega Man 2 (USA).nes | Mega Man 2 | 1 | Pending | MMC1 |
| Super Mario Bros. 3 (USA).nes | SMB3 | 4 | Pending | MMC3 |
| Final Fantasy (USA).nes | Final Fantasy | 1 | Pending | MMC1 |
| Dragon Warrior (USA).nes | Dragon Warrior | 1 | Pending | MMC1 |
| Castlevania (USA).nes | Castlevania | 0 | Pending | NROM |
| Contra (USA).nes | Contra | 1 | Pending | MMC1 |

## Status Categories

| Status | Description |
|--------|-------------|
| ✅ SUCCESS | ROM recompiled and runs without crashes |
| ⚠️ PLAYABLE | ROM runs and is playable through |
| ❌ RECOMPILE_FAIL | Recompiler failed to generate code |
| ⚠️ RUN_TIMEOUT | Game timed out during execution |
| 🔧 EXCEPTION | Runtime exception occurred |
| 🎨 GRAPHICS | Graphics glitches present |
| 🔊 AUDIO | Audio issues present |
| 🎮 INPUT | Input not working correctly |

## Mapper Support

| Mapper | Name | Games | Status |
|--------|------|-------|--------|
| 0 | NROM | SMB, Castlevania | Pending |
| 1 | MMC1 | Zelda, Metroid, MM2 | Pending |
| 2 | UxROM | TBD | Not started |
| 3 | CNROM | TBD | Not started |
| 4 | MMC3 | SMB3, Megaman 3+ | Not started |

## How to Contribute

1. **Test a ROM**: Run your ROM through the recompiler
2. **Report Results**: Add your results to the table above
3. **Include Details**:
   - ROM title and region
   - Mapper number (from ROM header)
   - Any issues encountered
   - MD5 hash (for ROM version identification)

## Testing Commands

```bash
# Recompile a ROM
./build/bin/nesrecomp roms/game.nes -o output/game

# Build the generated project
cmake -G Ninja -S output/game -B output/game/build
ninja -C output/game/build

# Run with tracing
./output/game/build/game --trace-entries game.trace

# Analyze coverage
python3 tools/compare_ground_truth.py --trace game.trace output/game
```

## Known Issues

- [ ] Mapper support incomplete
- [ ] Some 6502 opcodes may not be fully accurate
- [ ] PPU timing may need adjustment
- [ ] APU quality varies by game

## Progress Tracking

See `plan/cookie.txt` for current development progress.
