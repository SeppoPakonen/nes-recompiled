# Phase 1: Static Detection

Detect ROM→RAM copy patterns during static analysis and pre-compile the destination code.

## Overview

Many NES games copy code from ROM to RAM during initialization. This phase detects those copy patterns at compile time and generates pre-compiled code for the RAM destinations.

## Tasks

| Task | Description | Status |
|------|-------------|--------|
| [task030](task030_overview.md) | Phase overview | 📋 |
| [task031](task031_detect_copy_patterns.md) | Detect copy patterns | ⏳ |
| [task032](task032_map_ram_destinations.md) | RAM→ROM mapping | ⏳ |
| [task033](task033_generate_ram_dispatch.md) | Generate dispatch code | ⏳ |

## How It Works

```
┌─────────────────────────────────────────────────────────┐
│                    Analyzer                              │
├─────────────────────────────────────────────────────────┤
│  1. Scan for copy patterns:                              │
│     LDA $8000,Y    ← Read from ROM                      │
│     STA $0600,Y    ← Write to RAM                       │
│     DEY / BNE loop                                      │
│                                                          │
│  2. Verify destination is executed:                      │
│     JMP $0600       ← Jump to copied code               │
│                                                          │
│  3. Create mapping:                                      │
│     RAM $0600 ← ROM $8000 (bank 7)                      │
│                                                          │
│  4. Generate dispatch:                                   │
│     if (addr >= 0x0600 && addr < 0x0700)                │
│         func_07_8000(ctx);  ← Pre-compiled ROM code     │
└─────────────────────────────────────────────────────────┘
```

## Expected Results

After Phase 1:
- Detected copy patterns logged in verbose mode
- RAM→ROM mappings stored in analysis result
- Generated code includes RAM dispatch
- No interpreter hits for detected RAM code

## Limitations

- Only detects static copy patterns
- Doesn't handle self-modifying code
- Doesn't handle variable source/destination
- May miss complex copy loops

→ Phase 2 handles these cases with dynamic recompilation.

## Dependencies

- Track 1: 6502 instruction decoding
- Track 4: MMC1 bank tracking
- Track 7: Basic analyzer infrastructure
