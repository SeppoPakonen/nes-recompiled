# Task 008: Mapper Interface

## Objective
Create mapper interface for NES ROM banking.

## Background
NES mappers handle:
- PRG ROM banking (16KB or 32KB)
- CHR ROM/RAM banking (8KB)
- Mirroring control
- IRQ generation (some mappers)

Common mappers:
- Mapper 0: NROM (no banking)
- Mapper 1: MMC1 (512 games, incl. Zelda, Metroid)
- Mapper 2: UxROM (256 games)
- Mapper 3: CNROM (64 games)
- Mapper 4: MMC3 (1000+ games, incl. SMB3, Megaman)

## Changes Required

### 1. Create `runtime/include/nesrt_mapper.h`
```c
typedef enum {
    MAPPER_NROM = 0,
    MAPPER_MMC1 = 1,
    MAPPER_UXROM = 2,
    MAPPER_CNROM = 3,
    MAPPER_MMC3 = 4,
} NESMapperType;

typedef struct {
    NESMapperType type;
    uint8_t prg_banks;
    uint8_t chr_banks;
    
    // Banking state
    uint8_t prg_bank;
    uint8_t chr_bank;
    
    // Mirroring
    uint8_t mirroring;  // 0=horizontal, 1=vertical, 2=four-screen
    
    // Mapper-specific state
    union {
        struct { /* MMC1 state */ } mmc1;
        struct { /* MMC3 state */ } mmc3;
    };
} NESMapper;
```

### 2. Create `runtime/src/mapper.c`
- `nes_mapper_init()`
- `nes_mapper_read()` - PRG/CHR reads
- `nes_mapper_write()` - bank switch writes
- `nes_mapper_get_prg_ptr()`
- `nes_mapper_get_chr_ptr()`

## Acceptance Criteria
- [ ] Mapper 0 (NROM) works
- [ ] Mapper interface extensible
- [ ] Bank switching works

## Dependencies
- Task 001 (NES Header - need mapper number)

## Estimated Effort
4-8 hours

## Reference
NES emulator mappers at `/home/sblo/Ohjelmat/NES/src/mappers/`
