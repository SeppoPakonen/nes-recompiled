# Track 8: RAM Code Handling

## Overview

NES games frequently copy code from ROM to RAM and execute it from there. This is common for:
- **Initialization routines** - Setup code copied to zero page
- **Interrupt handlers** - NMI/IRQ handlers moved to RAM for speed
- **Dynamic code** - Generated or modified code at runtime
- **Bank switching stubs** - Small routines for mapper control

The static recompiler cannot analyze RAM code at compile time, so we need a hybrid approach combining:
1. Static detection of ROM→RAM copy patterns
2. Dynamic recompilation with hash-based caching
3. Trace-guided analysis of RAM execution

## Goals

1. **Detect ROM→RAM copies** during static analysis and pre-compile destination
2. **Dynamic recompilation** with hash-based caching for runtime code copies
3. **Trace RAM execution** to discover additional code paths
4. **Minimal performance impact** - cached code should execute as fast as ROM code

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Recompiled Executable                         │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐ │
│  │ ROM Code    │  │ RAM Cache   │  │ Dynamic Recompiler      │ │
│  │ (pre-compiled│  │ (hash →     │  │ (embedded in executable)│ │
│  │  functions) │  │  compiled)  │  │                         │ │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘ │
│         │                │                      │               │
│         └────────────────┴──────────────────────┘               │
│                          │                                      │
│                  ┌───────▼───────┐                              │
│                  │ Code Dispatch │                              │
│                  │ (check cache, │                              │
│                  │  compile if   │                              │
│                  │  needed)      │                              │
│                  └───────────────┘                              │
└─────────────────────────────────────────────────────────────────┘
```

## Implementation Phases

### Phase 1: Static Detection
Detect ROM→RAM copy patterns during analysis and pre-compile the destination addresses.

### Phase 2: Dynamic Recompilation
Implement runtime recompilation with hash-based caching for code copied to RAM.

### Phase 3: Trace Integration
Extend trace-guided analysis to capture and use RAM execution traces.

## Success Criteria

- [ ] Parasol Stars runs without hitting interpreter for RAM code
- [ ] Cached RAM code executes at native speed (no interpreter overhead)
- [ ] Cache hit rate >90% for typical games (most RAM code is copied from ROM)
- [ ] Executable size increase <50% (reasonable for embedded recompiler)

## Related Tracks

- **Track 1 (CPU)**: 6502 instruction handling
- **Track 7 (Integration)**: Build system and first ROM
- **Track 4 (Mappers)**: MMC1 banking (affects which ROM code is visible)
