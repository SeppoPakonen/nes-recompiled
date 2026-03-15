# Task 031: Detect ROM→RAM Copy Patterns

## Objective

Implement pattern detection in the analyzer to identify when code is copied from ROM to RAM.

## Implementation

### Location
`recompiler/src/analyzer.cpp`

### Algorithm

1. **Scan for copy loops**: Look for instruction sequences matching copy patterns
2. **Verify destination is executed**: Check if there's a JMP/JSR to the destination
3. **Record mapping**: Store ROM→RAM mapping for later use

### Pattern Matching

```cpp
struct CopyPattern {
    uint16_t src_start;      // ROM source address
    uint16_t dst_start;      // RAM destination address  
    uint16_t length;         // Bytes copied
    uint8_t src_bank;        // ROM bank
    bool verified;           // Destination is executed
};

// Common patterns to detect:
// 1. LDA abs,X / STA zpg,X loop
// 2. LDA (ptr),Y / STA zpg,Y loop
// 3. LDA abs,Y / STA zpg,Y loop
```

### Integration Points

- Run after main analysis pass
- Before IR generation
- Output: `std::vector<CopyPattern>` for use in codegen

## Acceptance Criteria

- [ ] Detects Parasol Stars startup copy ($8000→$0600)
- [ ] Doesn't flag data copies (only code destinations)
- [ ] Handles MMC1 bank switching context
- [ ] Logs detected patterns in verbose mode

## Testing

```bash
./build/bin/nesrecomp roms/a.nes -o output/test --verbose 2>&1 | grep "copy pattern"
# Expected: Detected copy pattern: $07:8000->$0600 (128 bytes)
```

## Dependencies

- Task 030: Phase 1 overview
- Track 1: Basic analyzer infrastructure
