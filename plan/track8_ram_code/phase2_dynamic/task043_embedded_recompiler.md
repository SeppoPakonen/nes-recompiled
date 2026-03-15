# Task 043: Embed Minimal Recompiler in Executable

## Objective

Include a minimal 6502 recompiler in the generated executable for runtime RAM code compilation.

## Implementation

### Components to Include

1. **Decoder** (`decoder_6502.c`) - Decode 6502 instructions
2. **IR Builder** (`ir_builder.c`) - Build IR from instructions
3. **JIT Emitter** (`jit_emitter.c`) - Generate machine code directly
4. **Linker** (`runtime_linker.c`) - Link generated code to runtime

### Size Optimization

- Strip debug info
- Use -Os optimization
- Only include needed instructions
- Pre-compile common patterns

### API

```c
typedef struct RAMRecompiler {
    uint8_t* code_buffer;      // Output buffer
    size_t buffer_size;        // Buffer size
    uint32_t cycles_estimate;  // Estimated cycles
} RAMRecompiler;

// Compile RAM code to function pointer
void* ram_recompile(NESContext* ctx, uint16_t ram_addr, uint16_t length);

// Free compiled code
void ram_free_compiled(void* func);
```

## Acceptance Criteria

- [ ] Executable includes recompiler (~50-100KB)
- [ ] Can compile 256 bytes of 6502 code in <10ms
- [ ] Generated code executes correctly
- [ ] Memory management works (no leaks)
