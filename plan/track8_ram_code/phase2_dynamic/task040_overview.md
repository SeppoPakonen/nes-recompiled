# Phase 2: Dynamic Recompilation with Hash Caching

## Objective

Implement runtime recompilation for RAM code that wasn't statically detected. Use hash-based caching to avoid recompiling the same code multiple times.

## Background

Not all RAM code can be detected statically:
- Self-modifying code
- Code copied with variable source/destination
- Code generated at runtime
- Missed copy patterns

For these cases, we need dynamic recompilation at runtime.

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                  Runtime Flow                                 │
├──────────────────────────────────────────────────────────────┤
│                                                               │
│  1. Game writes to RAM (STA instruction)                     │
│  2. Game jumps to RAM (JMP/JSR)                              │
│  3. Runtime intercepts:                                       │
│     a. Hash RAM contents                                      │
│     b. Check cache for existing compilation                   │
│     c. If miss: recompile and cache                           │
│     d. Dispatch to compiled code                              │
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

## Key Design Decisions

### 1. Hash Algorithm

Use fast hash for RAM contents:
- **FNV-1a**: Fast, good distribution, simple implementation
- **Range**: Hash only the code region being executed
- **Granularity**: Per-function or per-block hashing

```c
uint64_t hash_ram(const uint8_t* ram, uint16_t start, uint16_t length) {
    uint64_t hash = 14695981039346656037ULL;
    for (uint16_t i = 0; i < length; i++) {
        hash ^= ram[start + i];
        hash *= 1099511628211ULL;
    }
    return hash;
}
```

### 2. Cache Structure

```c
typedef struct RAMCodeCache {
    uint64_t hash;              // Hash of RAM contents
    void (*compiled_func)(NESContext*);  // Function pointer
    uint16_t ram_addr;          // RAM address range start
    uint16_t length;            // Code length
    struct RAMCodeCache* next;  // Hash collision chain
} RAMCodeCache;

// Global cache (embedded in executable)
RAMCodeCache* ram_code_cache[256];  // Hash table
```

### 3. Recompiler Integration

The executable includes a minimal recompiler:
- 6502 decoder
- IR builder
- C emitter (or direct machine code)
- Runtime linking

### 4. Performance Optimization

- **Compile ahead**: Detect writes to RAM, compile before first jump
- **Batch compilation**: Compile entire RAM region at once
- **Lazy compilation**: Only compile when jumped to
- **Pre-populated cache**: Include common patterns in executable

## Tasks

- [ ] **task041_runtime_cache.md**: Implement RAM code cache data structure
- [ ] **task042_hash_detection.md**: Implement RAM write detection and hashing
- [ ] **task043_embedded_recompiler.md**: Embed minimal recompiler in executable
- [ ] **task044_dispatch_integration.md**: Integrate with main dispatch loop
- [ ] **task045_performance_optimization.md**: Optimize cache lookup and compilation

## Expected Results

After Phase 2:
- Parasol Stars runs entirely in compiled code
- Cache hit rate >90% (most RAM code is copied from ROM)
- Performance within 2x of pure ROM execution
- Executable size increase <50%

## Memory Overhead

```
Cache entry: 32 bytes (hash + pointer + metadata)
Typical game: 10-50 unique RAM code regions
Total overhead: ~1.6KB for cache structure
Recompiler code: ~50-100KB (can be stripped for release)
```

## Dependencies

- Phase 1: Static detection (provides initial cache population)
- Track 1: 6502 decoder and IR builder
- Track 7: Build system for embedding recompiler
