# Phase 2: Dynamic Recompilation

Runtime compilation of RAM code with hash-based caching for optimal performance.

## Overview

Not all RAM code can be detected statically. This phase implements runtime recompilation with intelligent caching to handle:
- Self-modifying code
- Variable copy destinations
- Generated code
- Missed static patterns

## Tasks

| Task | Description | Status |
|------|-------------|--------|
| [task040](task040_overview.md) | Phase overview | 📋 |
| [task041](task041_runtime_cache.md) | RAM code cache | ⏳ |
| [task042](task042_hash_detection.md) | Write detection & hashing | ⏳ |
| [task043](task043_embedded_recompiler.md) | Embedded recompiler | ⏳ |

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                  Runtime Flow                                 │
├──────────────────────────────────────────────────────────────┤
│                                                               │
│  Game writes to RAM → Hash contents → Check cache           │
│                                                               │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────┐  │
│  │ RAM Write   │───▶│ Hash Check  │───▶│ Cache Hit?      │  │
│  └─────────────┘    └─────────────┘    └────────┬────────┘  │
│                                                  │           │
│                    ┌─────────────┐    ┌──────────▼────────┐ │
│                    │ Execute     │◀───│ Yes: Use Cached   │ │
│                    │ Compiled    │    └───────────────────┘ │
│                    └────▲───────┘                            │
│                         │                                     │
│                    ┌────┴────────┐    ┌─────────────────┐   │
│                    │ Recompile   │◀───│ No: Compile New │   │
│                    │ and Cache   │    └─────────────────┘   │
│                    └─────────────┘                           │
└──────────────────────────────────────────────────────────────┘
```

## Key Features

### Hash-Based Caching

```c
// FNV-1a hash for fast computation
uint64_t hash = fnv1a_hash(ram + addr, length);

// Cache lookup by hash
RAMCodeEntry* entry = ram_cache_lookup(&cache, addr, hash);
```

### Embedded Recompiler

The executable includes a minimal recompiler:
- 6502 decoder
- IR builder
- JIT emitter (direct machine code)
- Runtime linker

### Performance Optimization

- Compile ahead (detect writes before jumps)
- Batch compilation (entire RAM regions)
- Lazy compilation (only when needed)
- Pre-populated cache (from Phase 1 & 3)

## Expected Results

After Phase 2:
- All RAM code executes in compiled form
- Cache hit rate >90%
- Performance within 2x of pure ROM
- Executable size increase <50%

## Memory Overhead

```
Cache entry:     32 bytes
Typical game:    10-50 unique RAM regions
Cache overhead:  ~1.6KB
Recompiler code: ~50-100KB
```

## Dependencies

- Phase 1: Static detection (initial cache population)
- Track 1: 6502 decoder and IR builder
- Track 7: Build system for embedding
