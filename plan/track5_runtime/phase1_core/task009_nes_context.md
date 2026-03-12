# Task 009: NES Runtime Context

## Objective
Create main NES runtime context structure.

## Changes Required

### 1. Create `runtime/include/nesrt.h`
```c
typedef struct {
    // CPU state
    uint8_t a, x, y, sp;
    uint16_t pc;
    
    // Flags
    uint8_t flag_n;  // Negative
    uint8_t flag_v;  // Overflow
    uint8_t flag_b;  // Break
    uint8_t flag_d;  // Decimal
    uint8_t flag_i;  // Interrupt disable
    uint8_t flag_z;  // Zero
    uint8_t flag_c;  // Carry
    
    // Timing
    uint64_t cycles;
    uint64_t frame_cycles;
    uint8_t frame_done;
    
    // Memory
    uint8_t* prg_rom;     // PRG ROM (from mapper)
    size_t prg_rom_size;
    uint8_t* chr_rom;     // CHR ROM (from mapper)
    size_t chr_rom_size;
    uint8_t ram[0x800];   // 2KB work RAM ($0000-$07FF)
    
    // Components
    NESMapper mapper;
    NESPPU* ppu;
    NESAPU* apu;
    
    // Platform
    void* platform;
    NESPlatformCallbacks callbacks;
    
    // Debug
    bool trace_enabled;
    FILE* trace_file;
} NESContext;

typedef struct {
    void (*on_vblank)(NESContext* ctx);
    void (*on_audio_sample)(NESContext* ctx, int16_t left, int16_t right);
    uint8_t (*get_input)(NESContext* ctx, uint8_t port);
    bool (*load_battery)(NESContext* ctx, const char* name, void* data, size_t size);
    bool (*save_battery)(NESContext* ctx, const char* name, const void* data, size_t size);
} NESPlatformCallbacks;
```

### 2. Create `runtime/src/nesrt.c`
- `nes_context_create()`
- `nes_context_destroy()`
- `nes_context_reset()`
- `nes_context_load_rom()`
- `nes_read8()`, `nes_write8()` - memory access
- `nes_read16()`, `nes_write16()`
- `nes_push8()`, `nes_pop8()`
- `nes_push16()`, `nes_pop16()`

### 3. ALU operations in `runtime/src/cpu_ops.c`
- `nes_adc()` - add with carry
- `nes_sbc()` - subtract with carry
- `nes_and()`, `nes_or()`, `nes_eor()`
- `nes_asl()`, `nes_lsr()`, `nes_rol()`, `nes_ror()`
- `nes_bit()` - test bits
- `nes_cmp()`, `nes_cpx()`, `nes_cpy()`
- Flag helpers: `nes_set_flags_zn()`, etc.

## Acceptance Criteria
- [ ] Context creates/destroys cleanly
- [ ] Memory map correct ($0000-$FFFF)
- [ ] All CPU ops implemented
- [ ] Links with generated code

## Dependencies
- Track 1 (CPU decoder/analyzer/emitter)
- Task 006 (PPU)
- Task 007 (APU)
- Task 008 (Mapper)

## Estimated Effort
16-24 hours
