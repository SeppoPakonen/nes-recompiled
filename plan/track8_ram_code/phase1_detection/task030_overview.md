# Phase 1: Static Detection of ROM→RAM Copies

## Objective

Detect when the ROM copies code to RAM during execution and pre-compile the destination addresses. This handles the common case where games copy initialization routines, interrupt handlers, or bank-switching stubs to RAM at startup.

## Background

Analysis of Parasol Stars (a.nes) shows:
- Game copies code from ROM to $0600-$0700 range during startup
- Code executes from RAM addresses (0x0600-0x0700)
- Static analyzer doesn't know this code will be in RAM
- Result: interpreter fallback for all RAM execution

## Approach

### 1. Pattern Detection in Analyzer

Scan for common ROM→RAM copy patterns:

```assembly
; Typical pattern:
LDY #$00
loop:
    LDA $8000,Y    ; Read from ROM
    STA $0600,Y    ; Write to RAM
    INY
    CPY #$80       ; Copy 128 bytes
    BNE loop
    JMP $0600      ; Jump to copied code
```

Detect:
- Sequential reads from ROM addresses
- Sequential writes to RAM addresses ($0000-$07FF)
- Same index register (X/Y) for both
- Jump/JSR to destination after copy

### 2. Pre-compile Destination

When a copy pattern is detected:
1. Record source ROM range (bank:address)
2. Record destination RAM range
3. Copy IR blocks from source to destination
4. Generate function for RAM destination

### 3. Generate RAM Dispatch Code

For each RAM region with pre-compiled code:
```c
// Generated dispatch for RAM
if (addr >= 0x0600 && addr < 0x0700) {
    uint16_t rom_offset = addr - 0x0600 + 0x8000;  // Map back to ROM
    uint8_t rom_bank = 7;  // Source bank
    dispatch_rom(rom_bank, rom_offset);
}
```

## Tasks

- [ ] **task031_detect_copy_patterns.md**: Implement pattern detection in analyzer
- [ ] **task032_map_ram_destinations.md**: Create RAM→ROM mapping data structure
- [ ] **task033_generate_ram_dispatch.md**: Generate dispatch code for RAM regions
- [ ] **task034_test_parasol_stars.md**: Test with Parasol Stars startup

## Expected Results

After Phase 1:
- Parasol Stars startup code at $0600 is pre-compiled
- No interpreter hits for $0600-$0700 range during startup
- Game progresses further before hitting uncached RAM code

## Implementation Notes

### Pattern Detection Algorithm

```cpp
struct CopyPattern {
    uint16_t src_start;
    uint16_t dst_start;
    uint16_t length;
    uint8_t src_bank;
};

std::vector<CopyPattern> detect_copy_patterns(
    const std::vector<Instruction>& instructions) {
    
    // Look for LDA/STA pairs with same index
    // Track sequential access patterns
    // Verify jump to destination after copy
}
```

### RAM Mapping

```cpp
struct RAMMapping {
    uint16_t ram_addr;      // RAM destination
    uint16_t rom_addr;      // ROM source
    uint8_t rom_bank;       // ROM bank
    uint16_t length;        // Copy length
};

std::map<uint16_t, RAMMapping> ram_mappings;
```

## Risks

1. **False positives**: Not all ROM→RAM transfers are code (could be data)
   - Mitigation: Only mark as code if destination is jumped to
   
2. **Self-modifying code**: Pre-compiled code won't match if modified
   - Mitigation: Phase 2 dynamic recompilation handles this

3. **Bank switching**: Source bank may change during execution
   - Mitigation: Track MMC1 state during analysis

## Dependencies

- Track 1 (CPU): 6502 instruction decoding
- Track 4 (Mappers): MMC1 bank tracking
- Track 7 (Integration): Basic recompiler infrastructure
