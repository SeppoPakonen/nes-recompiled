# Task 015: Update GROUND_TRUTH_WORKFLOW.md for NES

## Objective
Adapt ground truth workflow documentation for NES.

## Changes Required

### Tool References
- Change PyBoy to NES emulator (e.g., PyNES, FCEUX)
- Change `pokeblue.gb` → NES ROM example
- Change `.trace` file format if needed

### Technical Details
- Update for 6502 instruction set differences
- Change bank:address format if needed (NES uses different banking)
- Update jump table examples (6502 uses different indirect jumps)

### Commands
- Update all command examples for `nesrecomp`
- Update trace file format references

## Acceptance Criteria
- [ ] Workflow applicable to NES
- [ ] Commands work with nesrecomp
- [ ] Tool recommendations accurate

## Dependencies
None

## Estimated Effort
2-4 hours
