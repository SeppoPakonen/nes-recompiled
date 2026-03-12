# Task 013: Update README.md for NES

## Objective
Update main README.md to reflect NES recompiler instead of GameBoy.

## Changes Required

### Title and Description
- Change "GB Recompiled" → "NES Recompiled"
- Change "GameBoy ROMs" → "NES ROMs"
- Change "Z80 assembly" → "6502 assembly"

### Features Section
- Update compatibility stats (TBD after testing)
- Change MBC support to Mapper support (MMC1, MMC3, etc.)
- Update PPU description (256x240, 60fps)
- Update APU description (5 channels: 2 pulse, triangle, noise, DMC)

### Quick Start
- Change `gbrecomp` → `nesrecomp`
- Change `.gb` → `.nes`
- Update binary names

### Controls
- Update controller mapping for NES (A, B, Select, Start, D-Pad)

### How It Works
- Update analysis section for 6502
- Change entry points (0xFFFC reset vector)
- Update runtime description (`libnesrt` instead of `libgbrt`)

### Compatibility
- Remove GameBoy test results
- Add NES test results table (TBD)

### Roadmap
- Update for NES-specific features
- Remove GameBoy Color references
- Add NES-specific goals (Famicom Disk System, etc.)

### Acknowledgments
- Change Pan Docs to NesDev Wiki
- Add reference to 6502 documentation
- Keep N64Recomp acknowledgment

## Acceptance Criteria
- [ ] All GameBoy references replaced with NES
- [ ] Build instructions work
- [ ] Screenshots updated (optional)
- [ ] Links updated

## Dependencies
None

## Estimated Effort
2-4 hours
