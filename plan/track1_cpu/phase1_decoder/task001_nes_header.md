# Task 001: NES ROM Header Parser

## Objective
Create NES ROM header parser to replace GameBoy ROM loader.

## Background
NES iNES format header (16 bytes):
- Bytes 0-3: "NES\x1A" magic
- Byte 4: PRG ROM size (16KB units)
- Byte 5: CHR ROM size (8KB units)
- Byte 6: Flags 6 (mapper bits 0-3, mirroring, battery, trainer)
- Byte 7: Flags 7 (mapper bits 4-7, VS/Playchoice, NES 2.0)
- Bytes 8-15: Reserved/extended

## Changes Required

### 1. Modify `recompiler/include/recompiler/rom.h`
- Rename namespace from `gbrecomp` to `nesrecomp`
- Replace `ROMHeader` struct with `NESHeader`:
  - PRG ROM size/banks
  - CHR ROM size/banks
  - Mapper number (0-511)
  - Mirroring mode (horizontal/vertical/four-screen)
  - Battery backup flag
  - Trainer flag
  - TV system (NTSC/PAL)

### 2. Modify `recompiler/src/rom.cpp`
- Update `ROM::load()` to parse iNES header
- Update validation logic for NES ROMs
- Handle NES 2.0 format (optional, phase 2)

## Acceptance Criteria
- [ ] Can load `/tmp/a.nes` and print header info
- [ ] Correctly identifies mapper number
- [ ] Correctly identifies PRG/CHR sizes
- [ ] Error handling for invalid headers

## Dependencies
None - foundational task

## Estimated Effort
2-4 hours
