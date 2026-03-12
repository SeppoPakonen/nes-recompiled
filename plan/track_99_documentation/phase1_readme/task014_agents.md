# Task 014: Update AGENTS.md for NES

## Objective
Update AGENTS.md agent rules for NES recompiler.

## Changes Required

### Project Overview
- Change "GameBoy ROMs" → "NES ROMs"
- Change `gbrecomp` → `nesrecomp`
- Change `gbrt` → `nesrt`
- Change `tetris_test` → NES test project

### Workflow Section
- Update test ROM recompilation commands
- Change `roms/tetris.gb` → `roms/a.nes`
- Update binary paths

### Debugging Section
- Update error messages for 6502
- Keep same debugging approach (--trace, --limit)

## Acceptance Criteria
- [ ] All paths and names updated
- [ ] Workflow commands work
- [ ] Debugging section accurate

## Dependencies
None

## Estimated Effort
1-2 hours
