# 🎮 Track 8: RAM Code Handling

Handle games that copy code from ROM to RAM and execute it from there.

## 🚀 Quick Start

```bash
# After implementation, recompile with RAM support:
./build/bin/nesrecomp roms/game.nes -o output/game --ram-cache

# Run with cache statistics:
./output/game/build/game --ram-cache-stats
```

## 📋 Overview

Many NES games copy code from ROM to RAM:
- **Initialization routines** - Setup code in zero page
- **Interrupt handlers** - NMI/IRQ handlers for speed
- **Bank switching stubs** - Mapper control routines
- **Dynamic code** - Generated or modified at runtime

The static recompiler can't analyze RAM code at compile time. Track 8 solves this with a three-phase approach:

```
┌─────────────────────────────────────────────────────────────┐
│                    Track 8 Architecture                      │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │ Phase 1     │  │ Phase 2     │  │ Phase 3             │ │
│  │ Static      │  │ Dynamic     │  │ Trace               │ │
│  │ Detection   │  │ Recompilation│ │ Integration         │ │
│  │             │  │ + Caching   │  │                     │ │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘ │
│         │                │                     │            │
│         └────────────────┼─────────────────────┘            │
│                          ▼                                  │
│              ┌───────────────────────┐                      │
│              │   Pre-compiled RAM    │                      │
│              │   + Runtime Cache     │                      │
│              │   + Trace Hints       │                      │
│              └───────────────────────┘                      │
└─────────────────────────────────────────────────────────────┘
```

## 📁 Phases

### 🔍 Phase 1: Static Detection

Detect ROM→RAM copy patterns during analysis and pre-compile destinations.

- [Overview](phase1_detection/README.md)
- [Task 031](phase1_detection/task031_detect_copy_patterns.md) - Detect copy patterns
- [Task 032](phase1_detection/task032_map_ram_destinations.md) - RAM→ROM mapping
- [Task 033](phase1_detection/task033_generate_ram_dispatch.md) - Generate dispatch code

**Status:** 📋 Planned

### ⚡ Phase 2: Dynamic Recompilation

Runtime compilation with hash-based caching.

- [Overview](phase2_dynamic/README.md)
- [Task 041](phase2_dynamic/task041_runtime_cache.md) - RAM code cache
- [Task 042](phase2_dynamic/task042_hash_detection.md) - Write detection & hashing
- [Task 043](phase2_dynamic/task043_embedded_recompiler.md) - Embedded recompiler

**Status:** 📋 Planned

### 📊 Phase 3: Trace Integration

Use execution traces to pre-populate cache.

- [Overview](phase3_tracing/README.md)
- [Task 051](phase3_tracing/task051_extend_trace_format.md) - Extended trace format
- [Task 052](phase3_tracing/task052_analyze_ram_traces.md) - Analyze RAM traces
- [Task 053](phase3_tracing/task053_prepopulate_cache.md) - Pre-populate cache

**Status:** 📋 Planned

### 📚 Phase 99: Documentation

Complete documentation for all phases.

- [Overview](phase99_docs/task990_overview.md)
- [Task 991](phase99_docs/task991_create_readme.md) - Main README
- [Task 992](phase99_docs/task992_document_architecture.md) - Architecture docs
- [Task 993](phase99_docs/task993_api_reference.md) - API reference
- [Task 994](phase99_docs/task994_integration_guide.md) - Integration guide
- [Task 995](phase99_docs/task995_update_overview.md) - Update overview

**Status:** 📋 Planned

## 🎯 Goals

| Goal | Target | Status |
|------|--------|--------|
| Parasol Stars runs without interpreter | 100% compiled | ⏳ |
| Cache hit rate | >90% | ⏳ |
| Performance overhead | <2x | ⏳ |
| Executable size increase | <50% | ⏳ |

## 📖 Documentation

- [Architecture](ARCHITECTURE.md) - Detailed design
- [API Reference](API.md) - Public API
- [Integration Guide](integration_guide.md) - How to integrate
- [Troubleshooting](troubleshooting.md) - Common issues

## 🔗 Related Tracks

- **Track 1** (CPU) - 6502 instruction handling
- **Track 4** (Mappers) - MMC1 banking
- **Track 7** (Integration) - Build system and first ROM

## 💡 Key Insight

Most RAM code is copied from ROM during startup. By hashing RAM contents and caching compiled code by hash:
- ROM copies will match existing cache entries (90%+ hit rate)
- Only truly dynamic code needs runtime compilation
- Dynamic code is cached after first execution

This gives near-native performance with minimal overhead! 🚀
