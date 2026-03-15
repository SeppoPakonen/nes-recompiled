# Phase 3: Trace Integration

Use execution traces to pre-populate the RAM code cache and avoid runtime compilation.

## Overview

Runtime compilation has overhead. By capturing RAM addresses during trace collection, we can pre-compile RAM code and include it in the generated executable.

## Tasks

| Task | Description | Status |
|------|-------------|--------|
| [task050](task050_overview.md) | Phase overview | 📋 |
| [task051](task051_extend_trace_format.md) | Extended trace format | ⏳ |
| [task052](task052_analyze_ram_traces.md) | Analyze RAM traces | ⏳ |
| [task053](task053_prepopulate_cache.md) | Pre-populate cache | ⏳ |

## Extended Trace Format

```
# Original format (ROM only)
7:C000
7:C035

# Extended format (includes RAM)
7:C000      # ROM: bank 7, address $C000
7:C035      # ROM: bank 7, address $C035
RAM:0600    # RAM: address $0600
RAM:0650    # RAM: address $0650
```

## Workflow

```
┌─────────────────────────────────────────────────────────────┐
│                   Trace Collection                           │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Run game with --trace-entries                              │
│  ↓                                                          │
│  Capture all executed addresses (ROM + RAM)                 │
│  ↓                                                          │
│  trace.log contains:                                        │
│    7:C000, 7:C035, RAM:0600, RAM:0650, ...                 │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   Trace Analysis                             │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Recompile with --use-trace trace.log                       │
│  ↓                                                          │
│  Analyzer processes RAM entries:                            │
│    - Find ROM source (via copy pattern detection)           │
│    - Create RAM→ROM mapping                                 │
│    - Mark for pre-compilation                               │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                 Executable Generation                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Generate pre-populated cache:                              │
│                                                              │
│  RAMCodeEntry ram_cache_init[] = {                          │
│      {0x12345678, func_07_8000, 0x0600, 128, 1, NULL},     │
│      {0x87654321, func_07_8100, 0x0700, 64, 1, NULL},      │
│      ...                                                    │
│  };                                                         │
│                                                              │
│  void init_ram_cache(ctx) {                                 │
│      for (entry : ram_cache_init)                           │
│          ram_cache_insert(&ctx->cache, ...);                │
│  }                                                          │
└─────────────────────────────────────────────────────────────┘
```

## Expected Results

After Phase 3:
- Trace files include RAM addresses
- Analyzer discovers RAM code from traces
- Generated executable has pre-populated cache
- Zero runtime compilation for traced games
- Cache hit rate approaches 100%

## Benefits

1. **No Runtime Overhead** - All code pre-compiled
2. **Deterministic Performance** - No compilation delays
3. **Smaller Executable** - No embedded recompiler needed (if all code traced)
4. **Better for Distribution** - End users don't need recompiler

## Limitations

1. **Requires Trace Collection** - Must run game first
2. **May Miss Edge Cases** - Unexecuted code paths not traced
3. **Version Specific** - Traces tied to specific ROM version

→ Combine with Phase 2 for complete coverage.

## Dependencies

- Phase 1: Static detection (provides initial mappings)
- Phase 2: Dynamic recompilation (provides cache infrastructure)
- Track 7: Trace-guided analysis infrastructure
