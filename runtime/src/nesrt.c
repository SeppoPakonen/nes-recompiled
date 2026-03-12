#include "nesrt.h"
#include "ppu.h"
#include "audio.h"
#include "audio_stats.h"
#include "platform_sdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nesrt_debug.h"

/* ============================================================================
 * Definitions
 * ========================================================================== */

/* NES memory sizes */
#define NES_WRAM_SIZE  0x0800  /* 2KB internal RAM */
#define NES_OAM_SIZE   0x0100  /* 256 bytes OAM */

/* ============================================================================
 * Globals
 * ========================================================================== */

bool nesrt_trace_enabled = false;
uint64_t nesrt_instruction_count = 0;
uint64_t nesrt_instruction_limit = 0;

static char* nesrt_trace_filename = NULL;


/* ============================================================================
 * Context Management
 * ========================================================================== */

/* NES memory sizes */
#define NES_WRAM_SIZE  0x0800  /* 2KB internal RAM */
#define NES_OAM_SIZE   0x0100  /* 256 bytes OAM */
#define NES_HRAM_SIZE  0x0080  /* 128 bytes HRAM (not used on NES) */
#define NES_IO_SIZE    0x0020  /* 32 bytes I/O ($4000-$4017, $2000-$2007) */

NESContext* nes_context_create(const NESConfig* config) {
    NESContext* ctx = (NESContext*)calloc(1, sizeof(NESContext));
    if (!ctx) return NULL;

    /* NES memory allocation */
    ctx->wram = (uint8_t*)calloc(1, NES_WRAM_SIZE);  /* 2KB internal RAM */
    ctx->oam = (uint8_t*)calloc(1, NES_OAM_SIZE);    /* 256 bytes OAM */
    ctx->io = (uint8_t*)calloc(1, 0x20);             /* I/O registers */

    if (!ctx->wram || !ctx->oam || !ctx->io) {
        nes_context_destroy(ctx);
        return NULL;
    }

    /* Note: VRAM is handled by PPU, not allocated here */
    ctx->vram = NULL;

    NESPPU* ppu = (NESPPU*)calloc(1, sizeof(NESPPU));
    if (ppu) {
        ppu_init(ppu);
        ctx->ppu = ppu;
    }

    ctx->apu = nes_audio_create();
    audio_stats_init();
    nes_context_reset(ctx, true);
    (void)config;

    if (nesrt_trace_filename) {
        ctx->trace_file = fopen(nesrt_trace_filename, "w");
        if (ctx->trace_file) {
            ctx->trace_entries_enabled = true;
            fprintf(stderr, "[NESRT] Tracing entry points to %s\n", nesrt_trace_filename);
        }
    }

    return ctx;
}

void nes_context_destroy(NESContext* ctx) {
    if (!ctx) return;

    /* Save RAM before destroying if available */
    if (ctx->mapper.prg_ram && ctx->mapper.prg_ram_enabled && ctx->callbacks.save_battery_ram) {
        /* Get ROM name for save file */
        char title[17] = {0};
        if (ctx->rom_size > 0x10) {
            /* iNES title is at offset 0x10-0x1F in header */
            memcpy(title, &ctx->rom[0x10], 16);
            for(int i=0; i<16; i++) {
                if(title[i] == 0 || title[i] < 32 || title[i] > 126) title[i] = 0;
            }
        }
        if(title[0] == 0) strcpy(title, "UNKNOWN_GAME");
        ctx->callbacks.save_battery_ram(ctx, title, ctx->mapper.prg_ram, ctx->mapper.prg_ram_size);
    }

    if (ctx->trace_file) fclose((FILE*)ctx->trace_file);
    free(ctx->wram);
    free(ctx->oam);
    free(ctx->io);

    /* Free mapper RAM */
    if (ctx->mapper.prg_ram) free(ctx->mapper.prg_ram);
    if (ctx->mapper.chr_ram) free(ctx->mapper.chr_ram);

    if (ctx->ppu) free(ctx->ppu);
    if (ctx->apu) nes_audio_destroy(ctx->apu);
    if (ctx->rom) free(ctx->rom);
    free(ctx);
}

void nes_context_reset(NESContext* ctx, bool skip_bootrom) {
    if (ctx->apu) {
        nes_audio_reset(ctx->apu);
    }

    /* Reset DMA state */
    ctx->dma.active = 0;
    ctx->dma.source_high = 0;
    ctx->dma.progress = 0;
    ctx->dma.cycles_remaining = 0;

    /* Reset 6502 registers to power-on state */
    ctx->pc = 0x0000;  /* Will be set from reset vector */
    ctx->sp = 0x01FD;  /* Stack pointer starts at $01FD */
    ctx->a = 0x00;
    ctx->x = 0x00;
    ctx->y = 0x00;
    ctx->f_z = 1;  /* Z flag set on power-up */
    ctx->f_n = 0;
    ctx->f_v = 0;
    ctx->f_c = 0;
    ctx->f_i = 1;  /* Interrupts disabled */
    ctx->f_d = 0;
    ctx->f_b = 0;
    ctx->f_h = 0;

    /* Reset interrupt state */
    ctx->ime = 0;
    ctx->ime_pending = 0;
    ctx->halted = 0;
    ctx->stopped = 0;
    ctx->halt_bug = 0;

    /* Clear internal RAM */
    memset(ctx->wram, 0, NES_WRAM_SIZE);

    /* Clear OAM */
    memset(ctx->oam, 0, NES_OAM_SIZE);

    /* Initialize I/O registers to default values */
    memset(ctx->io, 0, 0x20);

    /* Reset mapper state */
    if (ctx->mapper.prg_ram) {
        memset(ctx->mapper.prg_ram, 0, ctx->mapper.prg_ram_size);
    }
    if (ctx->mapper.chr_ram) {
        memset(ctx->mapper.chr_ram, 0, ctx->mapper.chr_ram_size);
    }

    /* Re-initialize mapper to default state */
    uint8_t prg_banks = (ctx->rom_size >= 32768) ? 2 : 1;
    nes_mapper_init(&ctx->mapper,
                    0,  /* Mapper number */
                    prg_banks,
                    1,  /* CHR banks */
                    ctx->mapper.prg_ram,
                    ctx->mapper.prg_ram_size,
                    ctx->mapper.chr_ram,
                    ctx->mapper.chr_ram_size);

    (void)skip_bootrom;
}

bool nes_context_load_rom(NESContext* ctx, const uint8_t* data, size_t size) {
    if (ctx->rom) free(ctx->rom);
    ctx->rom = (uint8_t*)malloc(size);
    if (!ctx->rom) return false;
    memcpy(ctx->rom, data, size);
    ctx->rom_size = size;

    /* Parse iNES header to get PRG/CHR sizes */
    /* iNES header: bytes 4 = PRG size (16KB units), byte 5 = CHR size (8KB units) */
    uint8_t prg_rom_units = data[4];
    uint8_t chr_rom_units = data[5];
    size_t prg_rom_size = prg_rom_units * 16 * 1024;
    size_t chr_rom_size = chr_rom_units * 8 * 1024;

    /* CHR ROM data starts after 16-byte header + PRG ROM */
    const uint8_t* chr_rom_data = NULL;
    if (chr_rom_units > 0 && size >= 16 + prg_rom_size + chr_rom_size) {
        chr_rom_data = &data[16 + prg_rom_size];
    }

    /* Initialize ROM data pointers for mapper */
    ctx->rom_data.prg_rom = ctx->rom;
    ctx->rom_data.prg_rom_size = prg_rom_size;
    ctx->rom_data.chr_rom = chr_rom_data;
    ctx->rom_data.chr_rom_size = chr_rom_size;

    /* Initialize mapper (default to NROM/mapper 0) */
    /* Note: Mapper number should come from iNES header parsing */
    /* For now, we default to mapper 0 with 16KB PRG and 8KB CHR */
    uint8_t prg_banks = (prg_rom_size >= 32768) ? 2 : 1;  /* 16KB or 32KB PRG */
    uint8_t chr_banks = (chr_rom_units > 0) ? chr_rom_units : 1;

    /* Allocate CHR RAM and copy CHR ROM data if present */
    uint8_t* chr_ram = NULL;
    size_t chr_ram_size = 0x2000;  /* 8KB CHR RAM */
    chr_ram = (uint8_t*)calloc(1, chr_ram_size);

    /* Copy CHR ROM data to CHR RAM */
    if (chr_rom_data && chr_rom_size > 0) {
        size_t copy_size = (chr_rom_size < chr_ram_size) ? chr_rom_size : chr_ram_size;
        memcpy(chr_ram, chr_rom_data, copy_size);
        DBG_PPU("Loaded %zu bytes of CHR ROM data", copy_size);
    }

    /* Allocate PRG RAM (8KB typical) */
    uint8_t* prg_ram = NULL;
    size_t prg_ram_size = 0x2000;  /* 8KB PRG RAM */
    prg_ram = (uint8_t*)calloc(1, prg_ram_size);

    /* Initialize mapper */
    nes_mapper_init(&ctx->mapper,
                    0,  /* Mapper 0 (NROM) - should come from header */
                    prg_banks,
                    chr_banks,
                    prg_ram,
                    prg_ram_size,
                    chr_ram,
                    chr_ram_size);

    /* Set CHR ROM data pointer to CHR RAM for mapper */
    ctx->mapper.chr_ram = chr_ram;
    ctx->mapper.chr_ram_size = chr_ram_size;
    ctx->mapper.chr_is_ram = true;

    /* Load CHR data into PPU VRAM */
    if (ctx->ppu && chr_ram && chr_ram_size > 0) {
        ppu_load_chr((NESPPU*)ctx->ppu, chr_ram, chr_ram_size);
    }

    /* Read reset vector from ROM ($FFFC-$FFFD) and set PC */
    /* The reset vector is always at the last 4 bytes of the PRG ROM data */
    /* File layout: 16-byte header + PRG ROM + CHR ROM */
    /* Reset vector is at: 16 + prg_rom_size - 4 */
    size_t reset_file_offset = 16 + prg_rom_size - 4;
    if (reset_file_offset + 2 <= size) {
        uint16_t reset_vector = data[reset_file_offset] | (data[reset_file_offset + 1] << 8);
        ctx->pc = reset_vector;
        DBG_PPU("Reset vector: 0x%04X (from file offset 0x%zX)", reset_vector, reset_file_offset);
    } else {
        /* Fallback: use last 4 bytes of file */
        ctx->pc = data[size - 2] | (data[size - 1] << 8);
        DBG_PPU("Reset vector: 0x%04X (from end of file)", ctx->pc);
    }

    return true;
}

bool nes_context_save_ram(NESContext* ctx) {
    if (!ctx || !ctx->eram || !ctx->eram_size || !ctx->callbacks.save_battery_ram) {
        return false;
    }
    
    /* Get ROM title for filename */
    char title[17] = {0};
    if (ctx->rom_size > 0x143) {
        memcpy(title, &ctx->rom[0x134], 16);
        for(int i=0; i<16; i++) {
            if(title[i] == 0 || title[i] < 32 || title[i] > 126) title[i] = 0;
        }
    }
    if(title[0] == 0) strcpy(title, "UNKNOWN_GAME");
    
    bool result = ctx->callbacks.save_battery_ram(ctx, title, ctx->eram, ctx->eram_size);
    if (result) {
        printf("[NESRT] Saved battery RAM for '%s'\n", title);
    } else {
        printf("[NESRT] Failed to save battery RAM for '%s'\n", title);
    }
    return result;
}

/* ============================================================================
 * Memory Access
 * ========================================================================== */

uint8_t nes_read8(NESContext* ctx, uint16_t addr) {
    /* During OAM DMA, only HRAM (0xFF80-0xFFFE) is accessible */
    if (ctx->dma.active && !(addr >= 0xFF80 && addr < 0xFFFF)) {
        return 0xFF;  /* Bus conflict - return undefined */
    }

    /* NES Memory Map:
     * $0000-$07FF: Internal RAM (2KB)
     * $0800-$1FFF: Mirrors of internal RAM
     * $2000-$2007: PPU registers
     * $2008-$3FFF: Mirrors of PPU registers
     * $4000-$4017: APU and I/O registers
     * $4018-$401F: APU and I/O (normally disabled)
     * $4020-$FFFF: Cartridge space
     */

    /* Internal RAM ($0000-$07FF, mirrored to $0800-$1FFF) */
    if (addr < 0x2000) {
        return ctx->wram[addr & 0x07FF];
    }

    /* PPU Registers ($2000-$2007, mirrored every 8 bytes to $3FFF) */
    if (addr < 0x4000) {
        if ((addr & 0x2007) >= 0x2000 && (addr & 0x2007) <= 0x2007) {
            return ppu_read_register((NESPPU*)ctx->ppu, ctx, addr & 0x2007);
        }
        return 0xFF;
    }

    /* APU and I/O Registers ($4000-$4017) */
    if (addr < 0x4018) {
        if (addr >= 0x4000 && addr <= 0x4017) {
            return nes_audio_read(ctx, addr);
        }
        return 0xFF;
    }

    /* Cartridge Space ($4020-$FFFF) */
    if (addr >= 0x4020) {
        /* PRG RAM ($6000-$7FFF for some mappers) */
        if (addr >= 0x6000 && addr < 0x8000) {
            if (ctx->mapper.prg_ram && ctx->mapper.prg_ram_enabled) {
                return nes_mapper_prg_ram_read(&ctx->mapper, addr);
            }
            return 0xFF;  /* Open bus */
        }

        /* PRG ROM ($8000-$FFFF) */
        if (addr >= 0x8000) {
            return nes_mapper_prg_read(&ctx->mapper, &ctx->rom_data, addr);
        }
    }

    /* Unmapped regions */
    return 0xFF;
}

void nes_write8(NESContext* ctx, uint16_t addr, uint8_t value) {
    /* During OAM DMA, only HRAM (0xFF80-0xFFFE) is writable */
    if (ctx->dma.active && !(addr >= 0xFF80 && addr < 0xFFFF)) {
        return;  /* Bus conflict - write ignored */
    }

    /* NES Memory Map:
     * $0000-$07FF: Internal RAM (2KB)
     * $0800-$1FFF: Mirrors of internal RAM
     * $2000-$2007: PPU registers
     * $2008-$3FFF: Mirrors of PPU registers
     * $4000-$4017: APU and I/O registers
     * $4018-$401F: APU and I/O (normally disabled)
     * $4020-$FFFF: Cartridge space
     */

    /* Internal RAM ($0000-$07FF, mirrored to $0800-$1FFF) */
    if (addr < 0x2000) {
        ctx->wram[addr & 0x07FF] = value;
        return;
    }

    /* PPU Registers ($2000-$2007, mirrored every 8 bytes to $3FFF) */
    if (addr < 0x4000) {
        if ((addr & 0x2007) >= 0x2000 && (addr & 0x2007) <= 0x2007) {
            ppu_write_register((NESPPU*)ctx->ppu, ctx, addr & 0x2007, value);
        }
        return;
    }

    /* APU and I/O Registers ($4000-$4017) */
    if (addr < 0x4018) {
        if (addr >= 0x4000 && addr <= 0x4017) {
            nes_audio_write(ctx, addr, value);
            return;
        }
        return;
    }

    /* Cartridge Space ($4020-$FFFF) */
    if (addr >= 0x4020) {
        /* PRG RAM ($6000-$7FFF for some mappers) */
        if (addr >= 0x6000 && addr < 0x8000) {
            if (ctx->mapper.prg_ram && ctx->mapper.prg_ram_enabled) {
                nes_mapper_prg_ram_write(&ctx->mapper, addr, value);
            }
            return;
        }

        /* Mapper Register Writes ($8000-$FFFF) */
        if (addr >= 0x8000) {
            nes_mapper_write(&ctx->mapper, addr, value);
            return;
        }
    }
}

uint16_t nes_read16(NESContext* ctx, uint16_t addr) {
    return (uint16_t)nes_read8(ctx, addr) | ((uint16_t)nes_read8(ctx, addr + 1) << 8);
}

void nes_write16(NESContext* ctx, uint16_t addr, uint16_t value) {
    nes_write8(ctx, addr, value & 0xFF);
    nes_write8(ctx, addr + 1, value >> 8);
}

void nes_push16(NESContext* ctx, uint16_t value) {
    ctx->sp -= 2;
    nes_write16(ctx, ctx->sp, value);
}

uint16_t nes_pop16(NESContext* ctx) {
    uint16_t val = nes_read16(ctx, ctx->sp);
    ctx->sp += 2;
    return val;
}

void nes_push8(NESContext* ctx, uint8_t value) {
    ctx->sp--;
    nes_write8(ctx, ctx->sp, value);
}

uint8_t nes_pop8(NESContext* ctx) {
    uint8_t val = nes_read8(ctx, ctx->sp);
    ctx->sp++;
    return val;
}

/* ============================================================================
 * ALU
 * ========================================================================== */

void nes_add8(NESContext* ctx, uint8_t value) {
    uint32_t res = (uint32_t)ctx->a + value;
    ctx->f_z = (res & 0xFF) == 0;
    ctx->f_n = 0;
    ctx->f_h = ((ctx->a & 0x0F) + (value & 0x0F)) > 0x0F;
    ctx->f_c = res > 0xFF;
    ctx->a = (uint8_t)res;
}
void nes_adc8(NESContext* ctx, uint8_t value) {
    uint8_t carry = ctx->f_c ? 1 : 0;
    uint32_t res = (uint32_t)ctx->a + value + carry;
    ctx->f_z = (res & 0xFF) == 0;
    ctx->f_n = 0;
    ctx->f_h = ((ctx->a & 0x0F) + (value & 0x0F) + carry) > 0x0F;
    ctx->f_c = res > 0xFF;
    ctx->a = (uint8_t)res;
}
void nes_sub8(NESContext* ctx, uint8_t value) {
    ctx->f_z = ctx->a == value;
    ctx->f_n = 1;
    ctx->f_h = (ctx->a & 0x0F) < (value & 0x0F);
    ctx->f_c = ctx->a < value;
    ctx->a -= value;
}
void nes_sbc8(NESContext* ctx, uint8_t value) {
    uint8_t carry = ctx->f_c ? 1 : 0;
    int res = (int)ctx->a - (int)value - carry;
    ctx->f_z = (res & 0xFF) == 0;
    ctx->f_n = 1;
    ctx->f_h = ((int)(ctx->a & 0x0F) - (int)(value & 0x0F) - (int)carry) < 0;
    ctx->f_c = res < 0;
    ctx->a = (uint8_t)res;
}
void nes_and8(NESContext* ctx, uint8_t value) { ctx->a &= value; ctx->f_z = ctx->a == 0; ctx->f_n = 0; ctx->f_h = 1; ctx->f_c = 0; }
void nes_or8(NESContext* ctx, uint8_t value) { ctx->a |= value; ctx->f_z = ctx->a == 0; ctx->f_n = 0; ctx->f_h = 0; ctx->f_c = 0; }
void nes_xor8(NESContext* ctx, uint8_t value) { ctx->a ^= value; ctx->f_z = ctx->a == 0; ctx->f_n = 0; ctx->f_h = 0; ctx->f_c = 0; }
void nes_cp8(NESContext* ctx, uint8_t value) {
    ctx->f_z = ctx->a == value;
    ctx->f_n = 1;
    ctx->f_h = (ctx->a & 0x0F) < (value & 0x0F);
    ctx->f_c = ctx->a < value;
}
uint8_t nes_inc8(NESContext* ctx, uint8_t val) {
    ctx->f_h = (val & 0x0F) == 0x0F;
    val++;
    ctx->f_z = val == 0;
    ctx->f_n = 0;
    return val;
}
uint8_t nes_dec8(NESContext* ctx, uint8_t val) {
    ctx->f_h = (val & 0x0F) == 0;
    val--;
    ctx->f_z = val == 0;
    ctx->f_n = 1;
    return val;
}
void nes_add16(NESContext* ctx, uint16_t val) {
    uint32_t res = (uint32_t)ctx->hl + val;
    ctx->f_n = 0;
    ctx->f_h = ((ctx->hl & 0x0FFF) + (val & 0x0FFF)) > 0x0FFF;
    ctx->f_c = res > 0xFFFF;
    ctx->hl = (uint16_t)res;
}
void nes_add_sp(NESContext* ctx, int8_t off) {
    ctx->f_z = 0; ctx->f_n = 0;
    ctx->f_h = ((ctx->sp & 0x0F) + (off & 0x0F)) > 0x0F;
    ctx->f_c = ((ctx->sp & 0xFF) + (off & 0xFF)) > 0xFF;
    ctx->sp += off;
}
void nes_ld_hl_sp_n(NESContext* ctx, int8_t off) {
    ctx->f_z = 0; ctx->f_n = 0;
    ctx->f_h = ((ctx->sp & 0x0F) + (off & 0x0F)) > 0x0F;
    ctx->f_c = ((ctx->sp & 0xFF) + (off & 0xFF)) > 0xFF;
    ctx->hl = ctx->sp + off;
}

uint8_t nes_rlc(NESContext* ctx, uint8_t v) { ctx->f_c = v >> 7; v = (v << 1) | ctx->f_c; ctx->f_z = v == 0; ctx->f_n = 0; ctx->f_h = 0; return v; }
uint8_t nes_rrc(NESContext* ctx, uint8_t v) { ctx->f_c = v & 1; v = (v >> 1) | (ctx->f_c << 7); ctx->f_z = v == 0; ctx->f_n = 0; ctx->f_h = 0; return v; }
uint8_t nes_rl(NESContext* ctx, uint8_t v) { uint8_t c = ctx->f_c; ctx->f_c = v >> 7; v = (v << 1) | c; ctx->f_z = v == 0; ctx->f_n = 0; ctx->f_h = 0; return v; }
uint8_t nes_rr(NESContext* ctx, uint8_t v) { uint8_t c = ctx->f_c; ctx->f_c = v & 1; v = (v >> 1) | (c << 7); ctx->f_z = v == 0; ctx->f_n = 0; ctx->f_h = 0; return v; }
uint8_t nes_sla(NESContext* ctx, uint8_t v) { ctx->f_c = v >> 7; v <<= 1; ctx->f_z = v == 0; ctx->f_n = 0; ctx->f_h = 0; return v; }
uint8_t nes_sra(NESContext* ctx, uint8_t v) { ctx->f_c = v & 1; v = (uint8_t)((int8_t)v >> 1); ctx->f_z = v == 0; ctx->f_n = 0; ctx->f_h = 0; return v; }
uint8_t nes_swap(NESContext* ctx, uint8_t v) { v = (uint8_t)((v << 4) | (v >> 4)); ctx->f_z = v == 0; ctx->f_n = 0; ctx->f_h = 0; ctx->f_c = 0; return v; }
uint8_t nes_srl(NESContext* ctx, uint8_t v) { ctx->f_c = v & 1; v >>= 1; ctx->f_z = v == 0; ctx->f_n = 0; ctx->f_h = 0; return v; }
void nes_bit(NESContext* ctx, uint8_t bit, uint8_t v) { ctx->f_z = !(v & (1 << bit)); ctx->f_n = 0; ctx->f_h = 1; }

void nes_rlca(NESContext* ctx) { ctx->a = nes_rlc(ctx, ctx->a); ctx->f_z = 0; }
void nes_rrca(NESContext* ctx) { ctx->a = nes_rrc(ctx, ctx->a); ctx->f_z = 0; }
void nes_rla(NESContext* ctx) { ctx->a = nes_rl(ctx, ctx->a); ctx->f_z = 0; }
void nes_rra(NESContext* ctx) { ctx->a = nes_rr(ctx, ctx->a); ctx->f_z = 0; }

void nes_daa(NESContext* ctx) {
   int a = ctx->a;
   if (!ctx->f_n) {
       if (ctx->f_h || (a & 0xF) > 9) a += 0x06;
       if (ctx->f_c || a > 0x9F) a += 0x60;
   } else {
       if (ctx->f_h) a = (a - 6) & 0xFF;
       if (ctx->f_c) a -= 0x60;
   }

   ctx->f_h = 0;
   if ((a & 0x100) == 0x100) ctx->f_c = 1;

   a &= 0xFF;
   ctx->f_z = (a == 0);
   ctx->a = (uint8_t)a;
}

/* ============================================================================
 * 6502-specific ALU Operations
 * ========================================================================== */

/* 6502 ADC - Add with Carry
 * Sets: N, V, Z, C
 * Formula: A = A + M + C
 * Overflow: V = (~(A ^ M) & (A ^ result)) & 0x80
 */
void nes6502_adc(NESContext* ctx, uint8_t value) {
    uint8_t carry = ctx->f_c ? 1 : 0;
    uint16_t sum = (uint16_t)ctx->a + value + carry;
    
    /* Set Carry flag */
    ctx->f_c = (sum > 0xFF) ? 1 : 0;
    
    /* Set Overflow flag (signed overflow) */
    /* V = (~(A ^ M) & (A ^ result)) & 0x80 */
    ctx->f_v = ((~(ctx->a ^ value) & (ctx->a ^ (uint8_t)sum)) & 0x80) != 0;
    
    /* Set result */
    ctx->a = (uint8_t)sum;
    
    /* Set N and Z flags */
    ctx->f_n = (ctx->a & 0x80) != 0;
    ctx->f_z = (ctx->a == 0);
}

/* 6502 SBC - Subtract with Carry
 * Sets: N, V, Z, C
 * Formula: A = A - M - (1-C)
 * Note: C=1 means no borrow, C=0 means borrow
 * Overflow: V = ((A ^ M) & (A ^ result)) & 0x80
 */
void nes6502_sbc(NESContext* ctx, uint8_t value) {
    uint8_t borrow = ctx->f_c ? 0 : 1;
    int16_t diff = (int16_t)ctx->a - value - borrow;
    
    /* Set Carry flag (inverted borrow) */
    ctx->f_c = (ctx->a >= value + borrow) ? 1 : 0;
    
    /* Set Overflow flag (signed overflow) */
    /* V = ((A ^ M) & (A ^ result)) & 0x80 */
    ctx->f_v = (((ctx->a ^ value) & (ctx->a ^ (uint8_t)diff)) & 0x80) != 0;
    
    /* Set result */
    ctx->a = (uint8_t)diff;
    
    /* Set N and Z flags */
    ctx->f_n = (ctx->a & 0x80) != 0;
    ctx->f_z = (ctx->a == 0);
}

/* 6502 BIT - Bit Test
 * Sets: N, V, Z
 * N = bit 7 of M, V = bit 6 of M, Z = A & M == 0
 */
void nes6502_bit(NESContext* ctx, uint8_t value) {
    ctx->f_n = (value & 0x80) != 0;
    ctx->f_v = (value & 0x40) != 0;
    ctx->f_z = ((ctx->a & value) == 0);
}

/* 6502 CMP - Compare A with value
 * Sets: N, Z, C
 * Formula: A - M (result discarded, flags set)
 */
void nes6502_cmp(NESContext* ctx, uint8_t value) {
    ctx->f_c = (ctx->a >= value);
    uint8_t result = ctx->a - value;
    ctx->f_n = (result & 0x80) != 0;
    ctx->f_z = (result == 0);
}

/* 6502 CPX - Compare X with value
 * Sets: N, Z, C
 */
void nes6502_cpx(NESContext* ctx, uint8_t value) {
    ctx->f_c = (ctx->x >= value);
    uint8_t result = ctx->x - value;
    ctx->f_n = (result & 0x80) != 0;
    ctx->f_z = (result == 0);
}

/* 6502 CPY - Compare Y with value
 * Sets: N, Z, C
 */
void nes6502_cpy(NESContext* ctx, uint8_t value) {
    ctx->f_c = (ctx->y >= value);
    uint8_t result = ctx->y - value;
    ctx->f_n = (result & 0x80) != 0;
    ctx->f_z = (result == 0);
}

/* 6502 AND - Logical AND
 * Sets: N, Z
 */
void nes6502_and(NESContext* ctx, uint8_t value) {
    ctx->a &= value;
    ctx->f_n = (ctx->a & 0x80) != 0;
    ctx->f_z = (ctx->a == 0);
}

/* 6502 ORA - Logical OR
 * Sets: N, Z
 */
void nes6502_ora(NESContext* ctx, uint8_t value) {
    ctx->a |= value;
    ctx->f_n = (ctx->a & 0x80) != 0;
    ctx->f_z = (ctx->a == 0);
}

/* 6502 EOR - Logical XOR
 * Sets: N, Z
 */
void nes6502_eor(NESContext* ctx, uint8_t value) {
    ctx->a ^= value;
    ctx->f_n = (ctx->a & 0x80) != 0;
    ctx->f_z = (ctx->a == 0);
}

/* 6502 ASL - Arithmetic Shift Left
 * Sets: N, Z, C
 * C = bit 7, result = value << 1
 */
void nes6502_asl(NESContext* ctx, uint8_t* value) {
    ctx->f_c = (*value & 0x80) != 0;
    *value <<= 1;
    ctx->f_n = (*value & 0x80) != 0;
    ctx->f_z = (*value == 0);
}

/* 6502 LSR - Logical Shift Right
 * Sets: N, Z, C
 * C = bit 0, result = value >> 1
 */
void nes6502_lsr(NESContext* ctx, uint8_t* value) {
    ctx->f_c = (*value & 0x01) != 0;
    *value >>= 1;
    ctx->f_n = 0;
    ctx->f_z = (*value == 0);
}

/* 6502 ROL - Rotate Left
 * Sets: N, Z, C
 * C = bit 7, result = (value << 1) | old_C
 */
void nes6502_rol(NESContext* ctx, uint8_t* value) {
    uint8_t old_c = ctx->f_c;
    ctx->f_c = (*value & 0x80) != 0;
    *value = (*value << 1) | old_c;
    ctx->f_n = (*value & 0x80) != 0;
    ctx->f_z = (*value == 0);
}

/* 6502 ROR - Rotate Right
 * Sets: N, Z, C
 * C = bit 0, result = (value >> 1) | (old_C << 7)
 */
void nes6502_ror(NESContext* ctx, uint8_t* value) {
    uint8_t old_c = ctx->f_c;
    ctx->f_c = (*value & 0x01) != 0;
    *value = (*value >> 1) | (old_c << 7);
    ctx->f_n = (*value & 0x80) != 0;
    ctx->f_z = (*value == 0);
}

/* 6502 Status Register packing/unpacking
 * Bit 7: N, Bit 6: V, Bit 5: -, Bit 4: B, Bit 3: D, Bit 2: I, Bit 1: Z, Bit 0: C
 */

void nes6502_set_sr(NESContext* ctx, uint8_t sr) {
    ctx->f_n = (sr & 0x80) != 0;
    ctx->f_v = (sr & 0x40) != 0;
    /* Bit 5 is unused */
    ctx->f_b = (sr & 0x10) != 0;
    ctx->f_d = (sr & 0x08) != 0;
    ctx->f_i = (sr & 0x04) != 0;
    ctx->f_z = (sr & 0x02) != 0;
    ctx->f_c = (sr & 0x01) != 0;
}

uint8_t nes6502_get_sr(NESContext* ctx) {
    uint8_t sr = 0x20; /* Bit 5 is always set */
    if (ctx->f_n) sr |= 0x80;
    if (ctx->f_v) sr |= 0x40;
    if (ctx->f_b) sr |= 0x10;
    if (ctx->f_d) sr |= 0x08;
    if (ctx->f_i) sr |= 0x04;
    if (ctx->f_z) sr |= 0x02;
    if (ctx->f_c) sr |= 0x01;
    return sr;
}

/* ============================================================================
 * Control Flow helpers
 * ========================================================================== */

void nes_ret(NESContext* ctx) { ctx->pc = nes_pop16(ctx); }
void nesrt_jump_hl(NESContext* ctx) { ctx->pc = ctx->hl; }
void nes_rst(NESContext* ctx, uint8_t vec) { nes_push16(ctx, ctx->pc); ctx->pc = vec; }

void nesrt_set_trace_file(const char* filename) {
    if (nesrt_trace_filename) free(nesrt_trace_filename);
    if (filename) nesrt_trace_filename = strdup(filename);
    else nesrt_trace_filename = NULL;
}

void nesrt_log_trace(NESContext* ctx, uint16_t bank, uint16_t addr) {
    if (ctx->trace_entries_enabled && ctx->trace_file) {
        fprintf((FILE*)ctx->trace_file, "%d:%04x\n", (int)bank, (int)addr);
    }
}

__attribute__((weak)) void nes_dispatch(NESContext* ctx, uint16_t addr) { 
    nesrt_log_trace(ctx, (addr < 0x4000) ? 0 : ctx->rom_bank, addr);
    ctx->pc = addr; 
    nes_interpret(ctx, addr); 
}

__attribute__((weak)) void nes_dispatch_call(NESContext* ctx, uint16_t addr) { 
    nesrt_log_trace(ctx, (addr < 0x4000) ? 0 : ctx->rom_bank, addr);
    ctx->pc = addr; 
}

/* ============================================================================
 * Timing & Hardware Sync
 * ========================================================================== */

static inline void nes_sync(NESContext* ctx) {
    uint32_t current = ctx->cycles;
    uint32_t delta = current - ctx->last_sync_cycles;
    if (delta > 0) {
        ctx->last_sync_cycles = current;
        if (ctx->ppu) ppu_tick((NESPPU*)ctx->ppu, ctx, delta);
    }
}

void nes_add_cycles(NESContext* ctx, uint32_t cycles) {
    ctx->cycles += cycles;
    ctx->frame_cycles += cycles;
}



static void nes_rtc_tick(NESContext* ctx, uint32_t cycles) {
    if (!ctx->rtc.active) return;
    
    /* Update RTC time */
    ctx->rtc.last_time += cycles;
    while (ctx->rtc.last_time >= 4194304) { /* 1 second at 4.194304 MHz */
        ctx->rtc.last_time -= 4194304;
        
        ctx->rtc.s++;
        if (ctx->rtc.s >= 60) {
            ctx->rtc.s = 0;
            ctx->rtc.m++;
            if (ctx->rtc.m >= 60) {
                ctx->rtc.m = 0;
                ctx->rtc.h++;
                if (ctx->rtc.h >= 24) {
                    ctx->rtc.h = 0;
                    uint16_t d = ctx->rtc.dl | ((ctx->rtc.dh & 1) << 8);
                    d++;
                    ctx->rtc.dl = d & 0xFF;
                    if (d > 0x1FF) {
                        ctx->rtc.dh |= 0x80; /* Overflow */
                        ctx->rtc.dh &= 0xFE; /* Clear 9th bit */
                    } else {
                        ctx->rtc.dh = (ctx->rtc.dh & 0xFE) | ((d >> 8) & 1);
                    }
                }
            }
        }
    }
}

/**
 * Process OAM DMA transfer
 * DMA takes 160 M-cycles (640 T-cycles), copying 1 byte per M-cycle
 */
static void nes_dma_tick(NESContext* ctx, uint32_t cycles) {
    if (!ctx->dma.active) return;
    
    /* Process DMA cycles */
    while (cycles > 0 && ctx->dma.active) {
        /* Each byte takes 4 T-cycles (1 M-cycle) */
        uint32_t byte_cycles = (cycles >= 4) ? 4 : cycles;
        cycles -= byte_cycles;
        ctx->dma.cycles_remaining -= byte_cycles;
        
        /* Copy one byte every 4 T-cycles */
        if (ctx->dma.progress < 160 && (ctx->dma.cycles_remaining % 4) == 0) {
            uint16_t src_addr = ((uint16_t)ctx->dma.source_high << 8) | ctx->dma.progress;
            /* Directly access ROM/RAM without triggering normal restrictions */
            uint8_t byte;
            if (src_addr < 0x8000) {
                /* ROM */
                if (src_addr < 0x4000) {
                    byte = ctx->rom[src_addr];
                } else {
                    byte = ctx->rom[(ctx->rom_bank * 0x4000) + (src_addr - 0x4000)];
                }
            } else if (src_addr < 0xA000) {
                /* VRAM */
                byte = ctx->vram[src_addr - 0x8000];
            } else if (src_addr < 0xC000) {
                /* External RAM */
                byte = ctx->eram ? ctx->eram[(ctx->ram_bank * 0x2000) + (src_addr - 0xA000)] : 0xFF;
            } else if (src_addr < 0xE000) {
                /* WRAM */
                if (src_addr < 0xD000) {
                    byte = ctx->wram[src_addr - 0xC000];
                } else {
                    byte = ctx->wram[(ctx->wram_bank * 0x1000) + (src_addr - 0xD000)];
                }
            } else {
                byte = 0xFF;
            }
            ctx->oam[ctx->dma.progress] = byte;
            ctx->dma.progress++;
        }
        
        /* Check if DMA is complete */
        if (ctx->dma.progress >= 160 || ctx->dma.cycles_remaining == 0) {
            ctx->dma.active = 0;
        }
    }
}

void nes_tick(NESContext* ctx, uint32_t cycles) {
    static uint32_t last_log = 0;
    
    // Check limit
    if (nesrt_instruction_limit > 0) {
        nesrt_instruction_count++;
        if (nesrt_instruction_count >= nesrt_instruction_limit) {
            printf("Instruction limit reached (%llu)\n", (unsigned long long)nesrt_instruction_limit);
            exit(0);
        }
    }

    if (nesrt_trace_enabled && ctx->cycles - last_log >= 10000) {
        last_log = ctx->cycles;
        fprintf(stderr, "[TICK] Cycles: %u, PC: 0x%04X, IME: %d, IF: 0x%02X, IE: 0x%02X\n", 
                ctx->cycles, ctx->pc, ctx->ime, ctx->io[0x0F], ctx->io[0x80]);
    }
    nes_add_cycles(ctx, cycles);
    
    /* RTC Tick */
    nes_rtc_tick(ctx, cycles);
    
    /* OAM DMA Tick */
    nes_dma_tick(ctx, cycles);

    /* Update DIV and TIMA */
    uint16_t old_div = ctx->div_counter;
    ctx->div_counter += (uint16_t)cycles;
    ctx->io[0x04] = (uint8_t)(ctx->div_counter >> 8);
    if (ctx->apu) nes_audio_div_tick(ctx->apu, old_div, ctx->div_counter);
    
    uint8_t tac = ctx->io[0x07];
    if (tac & 0x04) { /* Timer Enabled */
        uint16_t mask;
        switch (tac & 0x03) {
            case 0: mask = 1 << 9; break; /* 4096 Hz (1024 cycles) -> bit 9 */
            case 1: mask = 1 << 3; break; /* 262144 Hz (16 cycles) -> bit 3 */
            case 2: mask = 1 << 5; break; /* 65536 Hz (64 cycles) -> bit 5 */
            case 3: mask = 1 << 7; break; /* 16384 Hz (256 cycles) -> bit 7 */
            default: mask = 0; break;
        }
        
        /* Check for falling edges.
           We detect how many times the bit flipped from 1 to 0.
           The bit flips every 'mask' cycles (period is 2*mask).
           We iterate to find all falling edges in the range. 
        */
        uint16_t current = old_div;
        uint32_t cycles_left = cycles;
        
        /* Optimization: if cycles are small (common case), doing a loop is fine. */
        while (cycles_left > 0) {
            /* Next falling edge is at next multiple of (2*mask) */
            uint16_t next_fall = (current | (mask * 2 - 1)) + 1;
            
            /* Distance to next fall */
            uint32_t dist = (uint16_t)(next_fall - current);
            if (dist == 0) dist = mask * 2; /* Should happen if current is exactly on edge? */
            
            /* Check if we reach the fall */
            if (cycles_left >= dist) {
                /* Validate it is a falling edge for the selected bit?
                   next_fall is the transition 11...1 -> 00...0 for bits < bit+1.
                   Bit 'mask' definitely transitions. 
                   Wait, next multiple of 2*mask means mask bit becomes 0.
                   So yes, next_fall is a falling edge point.
                */
                if (ctx->io[0x05] == 0xFF) { 
                    ctx->io[0x05] = ctx->io[0x06]; /* Reload TMA */
                    ctx->io[0x0F] |= 0x04;         /* Request Timer Interrupt */
                } else {
                    ctx->io[0x05]++;
                }
                current += (uint16_t)dist;
                cycles_left -= dist;
            } else {
                break;
            }
        }
    }
    
    if ((ctx->cycles & 0xFF) < cycles || (ctx->ime && (ctx->io[0x0F] & ctx->io[0x80] & 0x1F))) {
        nes_sync(ctx);
        if (ctx->frame_done || (ctx->ime && (ctx->io[0x0F] & ctx->io[0x80] & 0x1F))) ctx->stopped = 1;
    }
    if (ctx->apu) nes_audio_step(ctx, cycles);
    if (ctx->ime_pending) { ctx->ime = 1; ctx->ime_pending = 0; }
}

void nes_handle_interrupts(NESContext* ctx) {
    if (!ctx->ime) return;
    uint8_t if_reg = ctx->io[0x0F];
    uint8_t ie_reg = ctx->io[0x80];
    uint8_t pending = if_reg & ie_reg & 0x1F;
    if (pending) {
        ctx->ime = 0; ctx->halted = 0;
        uint16_t vec = 0; uint8_t bit = 0;
        if (pending & 0x01) { vec = 0x0040; bit = 0x01; }
        else if (pending & 0x02) { vec = 0x0048; bit = 0x02; }
        else if (pending & 0x04) { vec = 0x0050; bit = 0x04; }
        else if (pending & 0x08) { vec = 0x0058; bit = 0x08; }
        else if (pending & 0x10) { vec = 0x0060; bit = 0x10; }
        if (vec) {
            ctx->io[0x0F] &= ~bit;
            
            /* ISR takes 5 M-cycles (20 T-cycles) as per Pan Docs:
             * - 2 M-cycles: Wait states (NOPs)
             * - 2 M-cycles: Push PC to stack (SP decremented twice, PC written)
             * - 1 M-cycle: Set PC to interrupt vector
             */
            nes_tick(ctx, 8);  /* 2 wait M-cycles */
            nes_push16(ctx, ctx->pc);
            nes_tick(ctx, 8);  /* 2 push M-cycles */
            ctx->pc = vec;
            nes_tick(ctx, 4);  /* 1 jump M-cycle */
            ctx->stopped = 1;
        }
    }
}

/* ============================================================================
 * Execution
 * ========================================================================== */

uint32_t nes_run_frame(NESContext* ctx) {
    nes_reset_frame(ctx);
    uint32_t start = ctx->cycles;

    while (!ctx->frame_done) {
        nes_handle_interrupts(ctx);
        
        /* Check for HALT exit condition (even if IME=0) */
        if (ctx->halted) {
             if (ctx->io[0x0F] & ctx->io[0x80] & 0x1F) {
                 ctx->halted = 0;
             }
        }
        
        ctx->stopped = 0;
        if (ctx->halted) nes_tick(ctx, 4);
        else nes_step(ctx);
        nes_sync(ctx);
    }
    return ctx->cycles - start;
}

uint32_t nes_step(NESContext* ctx) {
    if (nesrt_instruction_limit > 0 && ++nesrt_instruction_count >= nesrt_instruction_limit) {
        printf("Instruction limit reached (%llu)\n", (unsigned long long)nesrt_instruction_limit);
        exit(0);
    }
    
    /* Handle HALT bug by falling back to interpreter for the next instruction */
    if (ctx->halt_bug) {
        nes_interpret(ctx, ctx->pc);
        return 0; /* Cycle counting handled by interpreter */
    }

    uint32_t start = ctx->cycles;
    nes_dispatch(ctx, ctx->pc);
    return ctx->cycles - start;
}

void nes_reset_frame(NESContext* ctx) {
    ctx->frame_done = 0;
    ctx->frame_cycles = 0;
    if (ctx->ppu) ppu_clear_frame_ready((NESPPU*)ctx->ppu);
}

const uint32_t* nes_get_framebuffer(NESContext* ctx) {
    if (ctx->ppu) return ppu_get_framebuffer((NESPPU*)ctx->ppu);
    return NULL;
}

void nes_halt(NESContext* ctx) { ctx->halted = 1; }
void nes_stop(NESContext* ctx) { ctx->stopped = 1; }
bool nes_frame_complete(NESContext* ctx) { return ctx->frame_done != 0; }

void nes_set_platform_callbacks(NESContext* ctx, const NESPlatformCallbacks* c) {
    if (ctx && c) {
        ctx->callbacks = *c;
    }
}

void nes_audio_callback(NESContext* ctx, int16_t l, int16_t r) {
    if (ctx && ctx->callbacks.on_audio_sample) {
        ctx->callbacks.on_audio_sample(ctx, l, r);
    }
}
